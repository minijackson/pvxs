/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pvxs/nt.h>
#include "utilpvt.h"

namespace pvxs {
namespace nt {

TypeDef TimeStamp::build()
{
    using namespace pvxs::members;

    TypeDef def(TypeCode::Struct, "time_t", {
                    Int64("secondsPastEpoch"),
                    Int32("nanoseconds"),
                    Int32("userTag"),
                });
    return def;
}

TypeDef Alarm::build()
{
    using namespace pvxs::members;

    TypeDef def(TypeCode::Struct, "alarm_t", {
                    Int32("severity"),
                    Int32("status"),
                    String("message"),
                });
    return def;
}

TypeDef NTScalar::build() const
{
    using namespace pvxs::members;

    if(!value.valid() || value.kind()==Kind::Compound)
        throw std::logic_error("NTScalar only permits (array of) primitive");

    TypeDef def(TypeCode::Struct,
                   value.isarray() ? "epics:nt/NTScalarArray:1.0" : "epics:nt/NTScalar:1.0", {
                       Member(value, "value"),
                       Struct("alarm", "alarm_t", {
                           Int32("severity"),
                           Int32("status"),
                           String("message"),
                       }),
                       TimeStamp{}.build().as("timeStamp"),
                   });

    const bool isnumeric = value.kind()==Kind::Integer || value.kind()==Kind::Real;
    const auto scalar = value.scalarOf();

    if(display && isnumeric) {
        def += {
                Struct("display", {
                           Member(scalar, "limitLow"),
                           Member(scalar, "limitHigh"),
                           String("description"),
                           String("units"),
                       }),
        };
        if(form) {
            def += {
                    Struct("display", {
                           Int32("precision"),
                           Struct("form", "enum_t", {
                               Int32("index"),
                               StringA("choices"),
                           }),
                    }),
            };
        }
    } else if(display && !isnumeric) {
            def += {
                    Struct("display", {
                               String("description"),
                               String("units"),
                           }),
            };
    }

    if(control && isnumeric) {
        def += {
                Struct("control", {
                           Member(scalar, "limitLow"),
                           Member(scalar, "limitHigh"),
                           Member(scalar, "minStep"),
                       }),
        };
    }

    if(valueAlarm && isnumeric) {
        def += {
                Struct("valueAlarm", {
                           Bool("active"),
                           Member(scalar, "lowAlarmLimit"),
                           Member(scalar, "lowWarningLimit"),
                           Member(scalar, "highWarningLimit"),
                           Member(scalar, "highAlarmLimit"),
                           Int32("lowAlarmSeverity"),
                           Int32("lowWarningSeverity"),
                           Int32("highWarningSeverity"),
                           Int32("highAlarmSeverity"),
                           Float64("hysteresis"),
                       }),
        };
    }

    return def;
}

TypeDef NTEnum::build() const
{
    using namespace pvxs::members;

    TypeDef def(TypeCode::Struct, "epics:nt/NTEnum:1.0", {
                    Struct("value", "enum_t", {
                        Int32("index"),
                        StringA("choices"),
                    }),
                    Alarm{}.build().as("alarm"),
                    TimeStamp{}.build().as("timeStamp"),
                    Struct("display", {
                        String("description"),
                    }),
                });

    return def;
}

struct NTTable::Pvt {
    struct Col {
        TypeCode code;
        std::string name, label;
    };
    std::vector<Col> cols;
};

NTTable::NTTable()
    :pvt(std::make_shared<Pvt>())
{}

NTTable::~NTTable() {}

NTTable& NTTable::add_column(TypeCode code,
                             const char *name,
                             const char *label)
{
    if(!code.valid() || code.isarray())
        throw std::logic_error(SB()<<"NTTable column "<<name<<" type must be scalar");
    Pvt::Col col{code.arrayOf(), name, label ? label : name};
    pvt->cols.emplace_back(std::move(col));
    return *this;
}

TypeDef NTTable::build() const
{
    std::vector<Member> columns;
    columns.reserve(pvt->cols.size());

    for(const auto& col : pvt->cols) {
        columns.emplace_back(col.code, col.name);
    }

    TypeDef def(TypeCode::Struct, "epics:nt/NTTable:1.0", {
                    members::StringA("labels"),
                    members::Struct("value", columns),
                    members::String("descriptor"), // ???
                    Alarm{}.build().as("alarm"),
                    TimeStamp{}.build().as("timeStamp"),
                });

    return def;
}

Value NTTable::create() const
{
    Value ret(build().create());
    shared_array<std::string> labels;
    labels.resize(pvt->cols.size());

    size_t i=0;
    for(const auto& col : pvt->cols) {
        labels[i++] = col.label;
    }
    ret["labels"] = labels.freeze();

    return ret;
}

TypeDef NTNDArray::build() const
{
    using namespace pvxs::members;

    auto time_t(TimeStamp{}.build());
    auto alarm_t = {
        Int32("severity"),
        Int32("status"),
        String("message"),
    };

    TypeDef def(TypeCode::Struct, "epics:nt/NTNDArray:1.0", {
                    Union("value", {
                        BoolA("booleanValue"),
                        Int8A("byteValue"),
                        Int16A("shortValue"),
                        Int32A("intValue"),
                        Int64A("longValue"),
                        UInt8A("ubyteValue"),
                        UInt16A("ushortValue"),
                        UInt32A("uintValue"),
                        UInt64A("ulongValue"),
                        Float32A("floatValue"),
                        Float64A("doubleValue"),
                    }),
                    Struct("codec", "codec_t", {
                        String("name"),
                        Any("parameters"),
                    }),
                    Int64("compressedSize"),
                    Int64("uncompressedSize"),
                    Int32("uniqueId"),
                    time_t.as("dataTimeStamp"),
                    Struct("alarm", "alarm_t", alarm_t),
                    time_t.as("timeStamp"),
                    StructA("dimension", "dimension_t", {
                        Int32("size"),
                        Int32("offset"),
                        Int32("fullSize"),
                        Int32("binning"),
                        Bool("reverse"),
                    }),
                    StructA("attribute", "epics:nt/NTAttribute:1.0", {
                        String("name"),
                        Any("value"),
                        StringA("tags"),
                        String("descriptor"),
                        Struct("alarm", "alarm_t", alarm_t),
                        time_t.as("timeStamp"),
                        Int32("sourceType"),
                        String("source"),
                    }),
                });

    return def;
}

NTURI::NTURI(std::initializer_list<Member> args)
{
    using namespace pvxs::members;

    _def = TypeDef(TypeCode::Struct, "epics:nt/NTURI:1.0", {
                       String("scheme"),
                       String("authority"),
                       String("path"),
                       Struct("query", args),
    });
}

}} // namespace pvxs::nt
