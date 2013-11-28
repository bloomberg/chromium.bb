// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_H_
#define MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_H_

#include <string.h>

#include <algorithm>

#include "mojo/public/bindings/lib/bindings_internal.h"

namespace mojo {

// Provides read-only access to array data.
template <typename T>
class Array {
 public:
  typedef internal::ArrayTraits<T, internal::TypeTraits<T>::kIsObject> Traits_;
  typedef typename Traits_::DataType Data;

  template <typename U>
  explicit Array<T>(const U& u, Buffer* buf = mojo::Buffer::current()) {
    Data* data = Data::New(u.size(), buf);
    memcpy(data->storage(), u.data(), u.size() * sizeof(T));
    data_ = data;
  }

  template <typename U>
  U To() const {
    assert(!internal::TypeTraits<T>::kIsObject);
    return U(data_->storage(), data_->storage() + data_->size());
  }

  bool is_null() const { return !data_; }

  size_t size() const { return data_->size(); }

  const T& at(size_t offset) const {
    return Traits_::ToConstRef(data_->at(offset));
  }
  const T& operator[](size_t offset) const { return at(offset); }

  // Provides a way to initialize an array element-by-element.
  class Builder {
   public:
    typedef typename Array<T>::Data Data;
    typedef typename Array<T>::Traits_ Traits_;

    explicit Builder(size_t num_elements, Buffer* buf = mojo::Buffer::current())
        : data_(Data::New(num_elements, buf)) {
    }

    size_t size() const { return data_->size(); }

    T& at(size_t offset) {
      return Traits_::ToRef(data_->at(offset));
    }
    T& operator[](size_t offset) { return at(offset); }

    Array<T> Finish() {
      Data* data = NULL;
      std::swap(data, data_);
      return internal::Wrap(data);
    }

   private:
    Data* data_;
  };

 protected:
  friend class internal::WrapperHelper<Array<T> >;

  struct Wrap {};
  Array(Wrap, const Data* data) : data_(data) {}

  const Data* data_;
};

// UTF-8 encoded
typedef Array<char> String;

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_H_
