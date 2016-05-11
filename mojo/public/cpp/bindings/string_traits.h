// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_H_

#include "base/logging.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"

namespace mojo {

// Access to a string inside a mojo message.
class StringDataView {
 public:
  explicit StringDataView(internal::String_Data* data) : data_(data) {}

  // Whether the data represents a null string.
  // Note: Must not access the following methods if this method returns true.
  bool is_null() const { return !data_; }

  const char* storage() const {
    DCHECK(!is_null());
    return data_->storage();
  }

  size_t size() const {
    DCHECK(!is_null());
    return data_->size();
  }

 private:
  internal::String_Data* data_;
};

// This must be specialized for any UserType to be serialized/deserialized as
// a mojom string.
//
// TODO(yzshen): This is work in progress. Add better documentation once the
// interface becomes more stable.
template <typename UserType>
struct StringTraits;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_H_
