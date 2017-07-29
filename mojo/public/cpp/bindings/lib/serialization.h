// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_H_

#include <string.h>

#include "base/numerics/safe_math.h"
#include "mojo/public/cpp/bindings/array_traits_carray.h"
#include "mojo/public/cpp/bindings/array_traits_stl.h"
#include "mojo/public/cpp/bindings/lib/array_serialization.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"
#include "mojo/public/cpp/bindings/lib/handle_interface_serialization.h"
#include "mojo/public/cpp/bindings/lib/map_serialization.h"
#include "mojo/public/cpp/bindings/lib/native_enum_serialization.h"
#include "mojo/public/cpp/bindings/lib/native_struct_serialization.h"
#include "mojo/public/cpp/bindings/lib/string_serialization.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/map_traits_stl.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/string_traits_stl.h"
#include "mojo/public/cpp/bindings/string_traits_string16.h"
#include "mojo/public/cpp/bindings/string_traits_string_piece.h"

namespace mojo {
namespace internal {

template <typename MojomType, typename UserType>
mojo::Message StructSerializeAsMessageImpl(UserType* input) {
  SerializationContext context;
  PrepareToSerialize<MojomType>(*input, &context);
  mojo::Message message;
  context.PrepareMessage(0, 0, &message);
  typename MojomTypeTraits<MojomType>::Data::BufferWriter writer;
  Serialize<MojomType>(*input, message.payload_buffer(), &writer, &context);
  return message;
}

template <typename MojomType, typename DataArrayType, typename UserType>
DataArrayType StructSerializeImpl(UserType* input) {
  static_assert(BelongsTo<MojomType, MojomTypeCategory::STRUCT>::value,
                "Unexpected type.");
  Message message = StructSerializeAsMessageImpl<MojomType>(input);
  uint32_t size = message.payload_num_bytes();
  DataArrayType result(size);
  if (size)
    memcpy(&result.front(), message.payload(), size);
  return result;
}

template <typename MojomType, typename UserType>
bool StructDeserializeImpl(const void* data,
                           size_t data_num_bytes,
                           std::vector<mojo::ScopedHandle> handles,
                           UserType* output,
                           bool (*validate_func)(const void*,
                                                 ValidationContext*)) {
  static_assert(BelongsTo<MojomType, MojomTypeCategory::STRUCT>::value,
                "Unexpected type.");
  using DataType = typename MojomTypeTraits<MojomType>::Data;

  const void* input_buffer = data_num_bytes == 0 ? nullptr : data;
  void* aligned_input_buffer = nullptr;

  // Validation code will insist that the input buffer is aligned, so we ensure
  // that here. If the input data is not aligned, we (sadly) copy into an
  // aligned buffer. In practice this should happen only rarely if ever.
  bool need_copy = !IsAligned(input_buffer);
  if (need_copy) {
    aligned_input_buffer = malloc(data_num_bytes);
    DCHECK(IsAligned(aligned_input_buffer));
    memcpy(aligned_input_buffer, data, data_num_bytes);
    input_buffer = aligned_input_buffer;
  }

  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(data_num_bytes));
  ValidationContext validation_context(
      input_buffer, static_cast<uint32_t>(data_num_bytes), handles.size(), 0);
  bool result = false;
  if (validate_func(input_buffer, &validation_context)) {
    SerializationContext context;
    *context.mutable_handles() = std::move(handles);
    result = Deserialize<MojomType>(
        reinterpret_cast<DataType*>(const_cast<void*>(input_buffer)), output,
        &context);
  }

  if (aligned_input_buffer)
    free(aligned_input_buffer);

  return result;
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_H_
