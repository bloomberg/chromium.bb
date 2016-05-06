// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_

#include <stddef.h>

#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/lib/wtf_string_serialization.h"
#include "mojo/public/cpp/bindings/native_struct.h"
#include "mojo/public/cpp/bindings/string_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

// This file is included by serialization implementation files to avoid circular
// includes.
// Users of the serialization funtions should include serialization.h (and also
// wtf_serialization.h if necessary).

namespace mojo {
namespace internal {

class Buffer;

class NativeStruct_Data;

struct SerializationContext;

template <typename MojomType, typename MaybeConstUserType>
struct Serializer;

// PrepareToSerialize() must be matched by a Serialize() for the same input
// later. Moreover, within the same SerializationContext if PrepareToSerialize()
// is called for |input_1|, ..., |input_n|, Serialize() must be called for
// those objects in the exact same order.
template <typename MojomType, typename InputUserType, typename... Args>
size_t PrepareToSerialize(InputUserType&& input, Args&&... args) {
  return Serializer<MojomType,
                    typename std::remove_reference<InputUserType>::type>::
      PrepareToSerialize(std::forward<InputUserType>(input),
                         std::forward<Args>(args)...);
}

template <typename MojomType, typename InputUserType, typename... Args>
void Serialize(InputUserType&& input, Args&&... args) {
  Serializer<MojomType, typename std::remove_reference<InputUserType>::type>::
      Serialize(std::forward<InputUserType>(input),
                std::forward<Args>(args)...);
}

template <typename MojomType,
          typename DataType,
          typename InputUserType,
          typename... Args>
bool Deserialize(DataType&& input, InputUserType* output, Args&&... args) {
  return Serializer<MojomType, InputUserType>::Deserialize(
      std::forward<DataType>(input), output, std::forward<Args>(args)...);
}

}  // namespace internal

// TODO(yzshen): The following functions eventually will become simple wrappers
// of the Serializer interface. Remove them.

// -----------------------------------------------------------------------------
// Forward declaration for native types.

size_t GetSerializedSize_(const NativeStructPtr& input,
                          internal::SerializationContext* context);

void Serialize_(NativeStructPtr input,
                internal::Buffer* buffer,
                internal::NativeStruct_Data** output,
                internal::SerializationContext* context);

bool Deserialize_(internal::NativeStruct_Data* input,
                  NativeStructPtr* output,
                  internal::SerializationContext* context);

// -----------------------------------------------------------------------------
// Forward declaration for String.

size_t GetSerializedSize_(const String& input,
                          internal::SerializationContext* context);
void Serialize_(const String& input,
                internal::Buffer* buffer,
                internal::String_Data** output,
                internal::SerializationContext* context);
bool Deserialize_(internal::String_Data* input,
                  String* output,
                  internal::SerializationContext* context);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_
