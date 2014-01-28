// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_TYPE_CONVERTER_H_
#define MOJO_PUBLIC_BINDINGS_TYPE_CONVERTER_H_

namespace mojo {

// Specialize to perform type conversion.
template <typename T, typename U> class TypeConverter {
  // static T ConvertFrom(const U& input, Buffer* buf);
  // static U ConvertTo(const T& input);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_TYPE_CONVERTER_H_
