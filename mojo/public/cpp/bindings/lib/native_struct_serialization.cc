// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/native_struct_serialization.h"

#include "mojo/public/cpp/bindings/lib/serialization.h"

namespace mojo {
namespace {

using Serializer = internal::Serializer<NativeStructPtr, const NativeStructPtr>;

}  // namespace

size_t GetSerializedSize_(const NativeStructPtr& input,
                          internal::SerializationContext* context) {
  return Serializer::PrepareToSerialize(input, context);
}

void Serialize_(NativeStructPtr input,
                internal::Buffer* buffer,
                internal::NativeStruct_Data** output,
                internal::SerializationContext* context) {
  return Serializer::Serialize(input, buffer, output, context);
}

bool Deserialize_(internal::NativeStruct_Data* input,
                  NativeStructPtr* output,
                  internal::SerializationContext* context) {
  return Serializer::Deserialize(input, output, context);
}

}  // namespace mojo
