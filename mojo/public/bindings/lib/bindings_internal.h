// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_
#define MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_

#include <stdint.h>

#include "mojo/public/system/macros.h"

namespace mojo {
template <typename T> class Array;

namespace internal {

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
  Array<T>* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(ArrayPointer<char>) == 8, bad_sizeof_ArrayPointer);

union StringPointer {
  uint64_t offset;
  Array<char>* ptr;
};
MOJO_COMPILE_ASSERT(sizeof(StringPointer) == 8, bad_sizeof_StringPointer);

#pragma pack(pop)

template <typename T>
struct ArrayTraits {
  typedef T StorageType;

  static T& ToRef(StorageType& e) { return e; }
  static T const& ToConstRef(const StorageType& e) { return e; }
};

template <typename P>
struct ArrayTraits<P*> {
  typedef StructPointer<P> StorageType;

  static P*& ToRef(StorageType& e) { return e.ptr; }
  static P* const& ToConstRef(const StorageType& e) { return e.ptr; }
};

template <typename T> class ObjectTraits {};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_
