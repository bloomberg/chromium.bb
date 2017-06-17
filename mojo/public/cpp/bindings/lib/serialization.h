// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_H_

#include <string.h>

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
#include "mojo/public/cpp/bindings/string_traits_stl.h"
#include "mojo/public/cpp/bindings/string_traits_string16.h"
#include "mojo/public/cpp/bindings/string_traits_string_piece.h"

namespace mojo {
namespace internal {

template <typename MojomType, typename DataArrayType, typename UserType>
DataArrayType StructSerializeImpl(UserType* input) {
  static_assert(BelongsTo<MojomType, MojomTypeCategory::STRUCT>::value,
                "Unexpected type.");

  SerializationContext context;
  size_t size = PrepareToSerialize<MojomType>(*input, &context);
  DCHECK_EQ(size, Align(size));

  DataArrayType result(size);
  if (size == 0)
    return result;

  void* result_buffer = &result.front();
  // The serialization logic requires that the buffer is 8-byte aligned. If the
  // result buffer is not properly aligned, we have to do an extra copy. In
  // practice, this should never happen for std::vector.
  bool need_copy = !IsAligned(result_buffer);

  if (need_copy) {
    // calloc sets the memory to all zero.
    result_buffer = calloc(size, 1);
    DCHECK(IsAligned(result_buffer));
  }

  Buffer buffer;
  buffer.Initialize(result_buffer, size);
  typename MojomTypeTraits<MojomType>::Data* data = nullptr;
  Serialize<MojomType>(*input, &buffer, &data, &context);

  if (need_copy) {
    memcpy(&result.front(), result_buffer, size);
    free(result_buffer);
  }

  return result;
}

template <typename MojomType, typename DataArrayType, typename UserType>
bool StructDeserializeImpl(const DataArrayType& input,
                           UserType* output,
                           bool (*validate_func)(const void*,
                                                 ValidationContext*)) {
  static_assert(BelongsTo<MojomType, MojomTypeCategory::STRUCT>::value,
                "Unexpected type.");
  using DataType = typename MojomTypeTraits<MojomType>::Data;

  // TODO(sammc): Use DataArrayType::empty() once WTF::Vector::empty() exists.
  void* input_buffer =
      input.size() == 0
          ? nullptr
          : const_cast<void*>(reinterpret_cast<const void*>(&input.front()));

  // Please see comments in StructSerializeImpl.
  bool need_copy = !IsAligned(input_buffer);

  if (need_copy) {
    input_buffer = malloc(input.size());
    DCHECK(IsAligned(input_buffer));
    memcpy(input_buffer, &input.front(), input.size());
  }

  ValidationContext validation_context(input_buffer, input.size(), 0, 0);
  bool result = false;
  if (validate_func(input_buffer, &validation_context)) {
    auto data = reinterpret_cast<DataType*>(input_buffer);
    SerializationContext context;
    result = Deserialize<MojomType>(data, output, &context);
  }

  if (need_copy)
    free(input_buffer);

  return result;
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_H_
