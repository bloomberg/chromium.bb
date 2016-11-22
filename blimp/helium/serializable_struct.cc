// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/serializable_struct.h"

namespace blimp {
namespace helium {

SerializableStruct::SerializableStruct() {}

SerializableStruct::~SerializableStruct() {}

bool SerializableStruct::Parse(
    google::protobuf::io::CodedInputStream* input_stream) {
  for (auto* field : fields_) {
    if (!field->Parse(input_stream)) {
      return false;
    }
  }
  return true;
}

void SerializableStruct::Serialize(
    google::protobuf::io::CodedOutputStream* output_stream) const {
  for (const auto* field : fields_) {
    field->Serialize(output_stream);
  }
}

SerializableStruct::FieldBase::FieldBase() {}

SerializableStruct::FieldBase::~FieldBase() {}

void SerializableStruct::FieldBase::RegisterWith(
    SerializableStruct* container) {
  container->fields_.push_back(this);
}

}  // namespace helium
}  // namespace blimp
