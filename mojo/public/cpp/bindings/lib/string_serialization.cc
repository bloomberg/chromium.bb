// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/string_serialization.h"

#include "mojo/public/cpp/bindings/lib/serialization.h"

namespace mojo {

size_t GetSerializedSize_(const String& input,
                          internal::SerializationContext* context) {
  return internal::PrepareToSerialize<String>(input, context);
}

void Serialize_(const String& input,
                internal::Buffer* buf,
                internal::String_Data** output,
                internal::SerializationContext* context) {
  internal::Serialize<String>(input, buf, output, context);
}

bool Deserialize_(internal::String_Data* input,
                  String* output,
                  internal::SerializationContext* context) {
  return internal::Deserialize<String>(input, output, context);
}

}  // namespace mojo
