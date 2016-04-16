// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_

namespace mojo {

// This must be specialized for any UserType to be serialized/deserialized as
// a mojom array.
//
// TODO(yzshen): This is work in progress. Add better documentation once the
// interface becomes more stable.
template <typename UserType>
struct ArrayTraits;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_
