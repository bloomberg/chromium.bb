// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_BAR_SERIALIZATION_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_BAR_SERIALIZATION_H_

#include "mojo/public/bindings/lib/bindings_serialization.h"

namespace sample {
class Bar;
}

namespace mojo {
namespace internal {

template <>
class ObjectTraits<sample::Bar> {
 public:
  static size_t ComputeSizeOf(const sample::Bar* bar);
  static sample::Bar* Clone(const sample::Bar* bar, Buffer* buf);
  static void EncodePointersAndHandles(sample::Bar* bar,
                                       std::vector<mojo::Handle>* handles);
  static bool DecodePointersAndHandles(sample::Bar* bar,
                                       const mojo::Message& message);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_BAR_SERIALIZATION_H_
