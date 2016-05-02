// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_FORWARD_H_

#include <stddef.h>

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/lib/wtf_string_serialization.h"
#include "mojo/public/cpp/bindings/map.h"
#include "mojo/public/cpp/bindings/native_struct.h"
#include "mojo/public/cpp/bindings/string_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

// This file is included by serialization implementation files to avoid circular
// includes.
// Users of the serialization funtions should include serialization.h (and also
// wtf_serialization.h if necessary).

namespace mojo {

template <typename T>
class WTFArray;

namespace internal {

template <typename T>
class Array_Data;

class ArrayValidateParams;

class Buffer;

template <typename K, typename V>
class Map_Data;

class NativeStruct_Data;

struct SerializationContext;

template <typename T>
struct ShouldUseNativeSerializer;

// -----------------------------------------------------------------------------
// Forward declaration for native types.

template <typename T>
size_t GetSerializedSizeNative_(const T& value, SerializationContext* context);

template <typename T>
void SerializeNative_(const T& value,
                      Buffer* buffer,
                      Array_Data<uint8_t>** out,
                      SerializationContext* context);

template <typename T>
bool DeserializeNative_(Array_Data<uint8_t>* data,
                        T* out,
                        SerializationContext* context);

template <typename MojomType, typename InputUserType>
struct Serializer;

}  // namespace internal

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
// Forward declaration for Array.

template <typename E>
inline size_t GetSerializedSize_(const Array<E>& input,
                                 internal::SerializationContext* context);

template <typename E>
inline void SerializeArray_(
    Array<E> input,
    internal::Buffer* buf,
    typename Array<E>::Data_** output,
    const internal::ArrayValidateParams* validate_params,
    internal::SerializationContext* context);

template <typename E>
inline bool Deserialize_(typename Array<E>::Data_* input,
                         Array<E>* output,
                         internal::SerializationContext* context);

// -----------------------------------------------------------------------------
// Forward declaration for WTFArray.

template <typename E>
inline size_t GetSerializedSize_(const WTFArray<E>& input,
                                 internal::SerializationContext* context);

template <typename E>
inline void SerializeArray_(
    WTFArray<E> input,
    internal::Buffer* buf,
    typename WTFArray<E>::Data_** output,
    const internal::ArrayValidateParams* validate_params,
    internal::SerializationContext* context);

template <typename E>
inline bool Deserialize_(typename WTFArray<E>::Data_* input,
                         WTFArray<E>* output,
                         internal::SerializationContext* context);

// -----------------------------------------------------------------------------
// Forward declaration for Map.

template <typename MapKey, typename MapValue>
inline size_t GetSerializedSize_(const Map<MapKey, MapValue>& input,
                                 internal::SerializationContext* context);

template <typename MapKey,
          typename MapValue,
          typename DataKey,
          typename DataValue>
inline void SerializeMap_(
    Map<MapKey, MapValue> input,
    internal::Buffer* buf,
    internal::Map_Data<DataKey, DataValue>** output,
    const internal::ArrayValidateParams* value_validate_params,
    internal::SerializationContext* context);

template <typename MapKey,
          typename MapValue,
          typename DataKey,
          typename DataValue>
inline bool Deserialize_(internal::Map_Data<DataKey, DataValue>* input,
                         Map<MapKey, MapValue>* output,
                         internal::SerializationContext* context);

// -----------------------------------------------------------------------------
// Forward declaration for String.

// TODO(yzshen): These methods are simply wrappers of the Serializer interface.
// Remove them.
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
