// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/lib/interface_id.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace internal {

template <typename T>
class Array_Data;

#pragma pack(push, 1)

struct StructHeader {
  uint32_t num_bytes;
  uint32_t version;
};
static_assert(sizeof(StructHeader) == 8, "Bad sizeof(StructHeader)");

struct ArrayHeader {
  uint32_t num_bytes;
  uint32_t num_elements;
};
static_assert(sizeof(ArrayHeader) == 8, "Bad_sizeof(ArrayHeader)");

template <typename T>
union StructPointer {
  uint64_t offset;
  T* ptr;
};
static_assert(sizeof(StructPointer<char>) == 8, "Bad_sizeof(StructPointer)");

template <typename T>
union ArrayPointer {
  uint64_t offset;
  Array_Data<T>* ptr;
};
static_assert(sizeof(ArrayPointer<char>) == 8, "Bad_sizeof(ArrayPointer)");

using StringPointer = ArrayPointer<char>;
static_assert(sizeof(StringPointer) == 8, "Bad_sizeof(StringPointer)");


template <typename T>
union UnionPointer {
  uint64_t offset;
  T* ptr;
};
static_assert(sizeof(UnionPointer<char>) == 8, "Bad_sizeof(UnionPointer)");

struct Interface_Data {
  MessagePipeHandle handle;
  uint32_t version;
};
static_assert(sizeof(Interface_Data) == 8, "Bad_sizeof(Interface_Data)");

struct AssociatedInterface_Data {
  InterfaceId interface_id;
  uint32_t version;
};
static_assert(sizeof(AssociatedInterface_Data) == 8,
              "Bad_sizeof(AssociatedInterface_Data)");

using AssociatedInterfaceRequest_Data = InterfaceId;

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

template <typename H>
struct IsHandle {
  enum { value = IsBaseOf<Handle, H>::value };
};

template <typename T>
struct IsUnionDataType {
 private:
  template <typename U>
  static YesType Test(const typename U::MojomUnionDataType*);

  template <typename U>
  static NoType Test(...);

  EnsureTypeIsComplete<T> check_t_;

 public:
  static const bool value =
      sizeof(Test<T>(0)) == sizeof(YesType) && !IsConst<T>::value;
};

template <typename T>
struct IsEnumDataType {
 private:
  template <typename U>
  static YesType Test(const typename U::MojomEnumDataType*);

  template <typename U>
  static NoType Test(...);

  EnsureTypeIsComplete<T> check_t_;

 public:
  static const bool value =
      sizeof(Test<T>(0)) == sizeof(YesType) && !IsConst<T>::value;
};

template <typename T, bool move_only = IsMoveOnlyType<T>::value>
struct WrapperTraits;

template <typename T>
struct WrapperTraits<T, false> {
  typedef T DataType;
};
template <typename H>
struct WrapperTraits<ScopedHandleBase<H>, true> {
  typedef H DataType;
};
template <typename S>
struct WrapperTraits<StructPtr<S>, true> {
  typedef typename S::Data_* DataType;
};
template <typename S>
struct WrapperTraits<InlinedStructPtr<S>, true> {
  typedef typename S::Data_* DataType;
};
template <typename S>
struct WrapperTraits<S, true> {
  typedef typename S::Data_* DataType;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_
