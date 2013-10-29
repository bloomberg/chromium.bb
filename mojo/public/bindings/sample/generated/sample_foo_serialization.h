// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_FOO_SERIALIZATION_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_FOO_SERIALIZATION_H_

#include "mojo/public/bindings/lib/bindings_serialization.h"

namespace sample {
class Foo;
}

namespace mojo {
namespace internal {

template <>
class ObjectTraits<sample::Foo> {
 public:
  static size_t ComputeSizeOf(const sample::Foo* foo);
  static sample::Foo* Clone(const sample::Foo* foo, Buffer* buf);
  static void EncodePointersAndHandles(sample::Foo* foo,
                                       std::vector<mojo::Handle>* handles);
  static bool DecodePointersAndHandles(sample::Foo* foo,
                                       const mojo::Message& message);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_FOO_SERIALIZATION_H_
