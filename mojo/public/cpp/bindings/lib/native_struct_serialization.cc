// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/native_struct_serialization.h"

#include "mojo/public/cpp/bindings/lib/serialization.h"

namespace mojo {

size_t GetSerializedSize_(const NativeStructPtr& input,
                          internal::SerializationContext* context) {
  return internal::PrepareToSerialize<NativeStructPtr>(input, context);
}

void Serialize_(NativeStructPtr input,
                internal::Buffer* buffer,
                internal::NativeStruct_Data** output,
                internal::SerializationContext* context) {
  internal::Serialize<NativeStructPtr>(input, buffer, output, context);
}

bool Deserialize_(internal::NativeStruct_Data* input,
                  NativeStructPtr* output,
                  internal::SerializationContext* context) {
  return internal::Deserialize<NativeStructPtr>(input, output, context);
}

}  // namespace mojo
