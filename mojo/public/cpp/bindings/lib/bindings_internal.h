// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/lib/interface_id.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/system/core.h"

namespace WTF {
class String;
}
namespace mojo {
class String;

template <typename T>
class StructPtr;

template <typename T>
class InlinedStructPtr;

namespace internal {

template <typename T>
class Array_Data;

using String_Data = Array_Data<char>;

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
union Pointer {
  uint64_t offset;
  T* ptr;
};
static_assert(sizeof(Pointer<char>) == 8, "Bad_sizeof(Pointer)");

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

template <typename MojomType, bool move_only = IsMoveOnlyType<MojomType>::value>
struct GetDataTypeAsArrayElement;

template <typename T>
struct GetDataTypeAsArrayElement<T, false> {
  using Data =
      typename std::conditional<std::is_enum<T>::value, int32_t, T>::type;
};
template <typename H>
struct GetDataTypeAsArrayElement<ScopedHandleBase<H>, true> {
  using Data = H;
};
template <typename S>
struct GetDataTypeAsArrayElement<StructPtr<S>, true> {
  using Data =
      typename std::conditional<IsUnionDataType<typename S::Data_>::value,
                                typename S::Data_,
                                typename S::Data_*>::type;
};
template <typename S>
struct GetDataTypeAsArrayElement<InlinedStructPtr<S>, true> {
  using Data =
      typename std::conditional<IsUnionDataType<typename S::Data_>::value,
                                typename S::Data_,
                                typename S::Data_*>::type;
};
template <typename S>
struct GetDataTypeAsArrayElement<S, true> {
  using Data =
      typename std::conditional<IsUnionDataType<typename S::Data_>::value,
                                typename S::Data_,
                                typename S::Data_*>::type;
};

template <>
struct GetDataTypeAsArrayElement<String, false> {
  using Data = String_Data*;
};

template <>
struct GetDataTypeAsArrayElement<WTF::String, false> {
  using Data = String_Data*;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_INTERNAL_H_
