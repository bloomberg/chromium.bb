// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_
#define MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_

#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace internal {
template <typename T> class Array_Data;

#pragma pack(push, 1)

struct StructHeader {
  uint32_t num_bytes;
  uint32_t num_fields;
};
MOJO_COMPILE_ASSERT(sizeof(StructHeader) == 8, bad_sizeof_StructHeader);

struct ArrayHeader {
  uint32_t num_bytes;
  uint32_t num_elements;
};
MOJO_COMPILE_ASSERT(sizeof(ArrayHeader) == 8, bad_sizeof_ArrayHeader);

template <typename T>
union StructPointer {
  uint64_t offset;
  T* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(StructPointer<char>) == 8, bad_sizeof_StructPointer);

template <typename T>
union ArrayPointer {
  uint64_t offset;
  Array_Data<T>* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(ArrayPointer<char>) == 8, bad_sizeof_ArrayPointer);

union StringPointer {
  uint64_t offset;
  Array_Data<char>* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(StringPointer) == 8, bad_sizeof_StringPointer);

#pragma pack(pop)

template <typename T>
void ResetIfNonNull(T* ptr) {
  if (ptr)
    *ptr = T();
}

template <typename T>
T FetchAndReset(T* ptr) {
  T temp = *ptr;
  *ptr = T();
  return temp;
}

template <typename T>
class WrapperHelper {
 public:
  static const T Wrap(const typename T::Data* data) {
    return T(typename T::Wrap(), const_cast<typename T::Data*>(data));
  }
  static typename T::Data* Unwrap(const T& object) {
    return const_cast<typename T::Data*>(object.data_);
  }
};

template <typename Data>
inline const typename Data::Wrapper Wrap(const Data* data) {
  return WrapperHelper<typename Data::Wrapper>::Wrap(data);
}

template <typename T>
inline typename T::Data* Unwrap(const T& object) {
  return WrapperHelper<T>::Unwrap(object);
}

template <typename T> struct TypeTraits {
  static const bool kIsObject = true;
};
template <> struct TypeTraits<bool> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<char> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<int8_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<int16_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<int32_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<int64_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<uint8_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<uint16_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<uint32_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<uint64_t> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<float> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<double> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<Handle> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<DataPipeConsumerHandle> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<DataPipeProducerHandle> {
  static const bool kIsObject = false;
};
template <> struct TypeTraits<MessagePipeHandle> {
  static const bool kIsObject = false;
};

template <typename T> class ObjectTraits {};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_
