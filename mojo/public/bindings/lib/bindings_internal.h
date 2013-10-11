// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_
#define MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_INTERNAL_H_

#include <stdint.h>

namespace mojo {
template <typename T> class Array;

namespace internal {

struct StructHeader {
  uint32_t num_bytes;
  uint32_t num_fields;
};

struct ArrayHeader {
  uint32_t num_bytes;
  uint32_t num_elements;
};

template <typename T>
union StructPointer {
  uint64_t offset;
  T* ptr;
};

template <typename T>
union ArrayPointer {
  uint64_t offset;
  Array<T>* ptr;
};

union StringPointer {
  uint64_t offset;
  Array<char>* ptr;
};

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
