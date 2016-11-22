// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_SERIALIZABLE_STRUCT_H_
#define BLIMP_HELIUM_SERIALIZABLE_STRUCT_H_

#include <memory>
#include <vector>

#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/serializable.h"
#include "blimp/helium/stream_helpers.h"
#include "blimp/helium/syncable.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

namespace blimp {
namespace helium {

// SerializableStruct is a base class which manages registration, serialization,
// and deserialization of its contents. Optional values are not permitted;
// SerializableStructs are therefore emitted/parsed in their entirety.
//
// Example usage:
//
//   struct MyStruct : public SerializableStruct {
//     MyStruct() : some_str(this, "initial value!"), some_int(this) {}
//     Field<std::string> some_str;
//     Field<int> some_int;
//   };
//
//   MyStruct s;
//   s.some_str.Set("foo");
//   s.some_int.Set(123);
class BLIMP_HELIUM_EXPORT SerializableStruct : public Serializable {
 public:
  // Provides template type-agnostic methods to SerializableStruct.
  class FieldBase : public Serializable {
   public:
    FieldBase();
    ~FieldBase();

    void SetLocalUpdateCallback(const base::Closure& local_update_callback);

    void RegisterWith(SerializableStruct* container);

   private:
    base::Closure local_update_callback_;
  };

  // Holds arbitrarily-typed Serializable values and registers itself with the
  // SerializableStruct.
  template <typename FieldType>
  class Field : public FieldBase {
   public:
    Field() = delete;

    explicit Field(SerializableStruct* changeset, const FieldType& value = {})
        : value_(value) {
      DCHECK(changeset);
      RegisterWith(changeset);
    }

    ~Field() {}

    void Set(const FieldType& value);
    const FieldType& operator()() const;

    // Serializable implementation.
    void Serialize(
        google::protobuf::io::CodedOutputStream* output_stream) const override;
    bool Parse(google::protobuf::io::CodedInputStream* input_stream) override;

   private:
    FieldType value_;

    DISALLOW_COPY_AND_ASSIGN(Field);
  };

  SerializableStruct();
  virtual ~SerializableStruct();

  void SetLocalUpdateCallback(const base::Closure& local_update_callback);

  // Serializable implementation.
  void Serialize(
      google::protobuf::io::CodedOutputStream* output_stream) const override;
  bool Parse(google::protobuf::io::CodedInputStream* input_stream) override;

 private:
  friend class FieldBase;

  std::vector<FieldBase*> fields_;

  DISALLOW_COPY_AND_ASSIGN(SerializableStruct);
};

template <typename FieldType>
void SerializableStruct::Field<FieldType>::Set(const FieldType& value) {
  value_ = value;
}

template <typename FieldType>
const FieldType& SerializableStruct::Field<FieldType>::operator()() const {
  return value_;
}

template <typename FieldType>
void SerializableStruct::Field<FieldType>::Serialize(
    google::protobuf::io::CodedOutputStream* output_stream) const {
  DCHECK(output_stream);
  WriteToStream(value_, output_stream);
}

template <typename FieldType>
bool SerializableStruct::Field<FieldType>::Parse(
    google::protobuf::io::CodedInputStream* input_stream) {
  DCHECK(input_stream);
  return ReadFromStream(input_stream, &value_);
}

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_SERIALIZABLE_STRUCT_H_
