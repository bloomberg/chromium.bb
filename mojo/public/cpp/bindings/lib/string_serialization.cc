// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/string_serialization.h"

#include "mojo/public/cpp/bindings/lib/serialization.h"

namespace mojo {
namespace {

using StringSerializer = internal::Serializer<String, const String>;

}  // namespace

size_t GetSerializedSize_(const String& input,
                          internal::SerializationContext* context) {
  return StringSerializer::PrepareToSerialize(input, context);
}

void Serialize_(const String& input,
                internal::Buffer* buf,
                internal::String_Data** output,
                internal::SerializationContext* context) {
  StringSerializer::Serialize(input, buf, output, context);
}

bool Deserialize_(internal::String_Data* input,
                  String* output,
                  internal::SerializationContext* context) {
  return StringSerializer::Deserialize(input, output, context);
}

}  // namespace mojo
