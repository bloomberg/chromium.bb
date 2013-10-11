// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_H_
#define MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_H_

#include <stddef.h>
#include <string.h>

#include <new>

#include "mojo/public/bindings/lib/bindings_internal.h"
#include "mojo/public/bindings/lib/buffer.h"
#include "mojo/public/system/core.h"

namespace mojo {

template <typename T>
class Array {
 public:
  static Array<T>* New(Buffer* buf, size_t num_elements) {
    size_t num_bytes = sizeof(Array<T>) + sizeof(StorageType) * num_elements;
    return new (buf->Allocate(num_bytes)) Array<T>(num_bytes, num_elements);
  }

  template <typename U>
  static Array<T>* NewCopyOf(Buffer* buf, const U& u) {
    Array<T>* result = Array<T>::New(buf, u.size());
    memcpy(result->storage(), u.data(), u.size() * sizeof(T));
    return result;
  }

  size_t size() const { return header_.num_elements; }

  T& at(size_t offset) {
    return internal::ArrayTraits<T>::ToRef(storage()[offset]);
  }

  const T& at(size_t offset) const {
    return internal::ArrayTraits<T>::ToConstRef(storage()[offset]);
  }

  T& operator[](size_t offset) {
    return at(offset);
  }

  const T& operator[](size_t offset) const {
    return at(offset);
  }

  template <typename U>
  U To() const {
    return U(storage(), storage() + size());
  }

 private:
  friend class internal::ObjectTraits<Array<T> >;

  typedef typename internal::ArrayTraits<T>::StorageType StorageType;

  StorageType* storage() {
    return reinterpret_cast<StorageType*>(this + 1);
  }
  const StorageType* storage() const {
    return reinterpret_cast<const StorageType*>(this + 1);
  }

  Array(size_t num_bytes, size_t num_elements) {
    header_.num_bytes = static_cast<uint32_t>(num_bytes);
    header_.num_elements = static_cast<uint32_t>(num_elements);
  }
  ~Array() {}

  internal::ArrayHeader header_;

  // Elements of type internal::ArrayTraits<T>::StorageType follow.
};

// UTF-8 encoded
typedef Array<char> String;

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_H_
