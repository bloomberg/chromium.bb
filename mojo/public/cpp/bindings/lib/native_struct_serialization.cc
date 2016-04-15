// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/native_struct_serialization.h"

#include "mojo/public/cpp/bindings/lib/serialization.h"

namespace mojo {

size_t GetSerializedSize_(const NativeStructPtr& input,
                          internal::SerializationContext* context) {
  if (!input)
    return 0;
  return GetSerializedSize_(input->data, context);
}

void Serialize_(NativeStructPtr input,
                internal::Buffer* buffer,
                internal::NativeStruct_Data** output,
                internal::SerializationContext* context) {
  if (!input) {
    *output = nullptr;
    return;
  }

  internal::Array_Data<uint8_t>* data = nullptr;
  const internal::ArrayValidateParams params(0, false, nullptr);
  SerializeArray_(std::move(input->data), buffer, &data, &params, context);
  *output = reinterpret_cast<internal::NativeStruct_Data*>(data);
}

bool Deserialize_(internal::NativeStruct_Data* input,
                  NativeStructPtr* output,
                  internal::SerializationContext* context) {
  internal::Array_Data<uint8_t>* data =
      reinterpret_cast<internal::Array_Data<uint8_t>*>(input);

  NativeStructPtr result(NativeStruct::New());
  if (!Deserialize_(data, &result->data, context)) {
    output = nullptr;
    return false;
  }
  if (!result->data)
    *output = nullptr;
  else
    result.Swap(output);
  return true;
}

}  // namespace mojo
