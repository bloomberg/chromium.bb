// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/sample/generated/sample_bar_serialization.h"

#include <string.h>

#include "mojo/public/bindings/sample/generated/sample_bar.h"

namespace mojo {
namespace internal {

// static
size_t ObjectTraits<sample::Bar>::ComputeSizeOf(
    const sample::Bar* bar) {
  return sizeof(*bar);
}

// static
sample::Bar* ObjectTraits<sample::Bar>::Clone(
    const sample::Bar* bar, Buffer* buf) {
  sample::Bar* clone = sample::Bar::New(buf);
  memcpy(clone, bar, sizeof(*bar));
  return clone;
}

// static
void ObjectTraits<sample::Bar>::EncodePointersAndHandles(
    sample::Bar* bar, std::vector<mojo::Handle>* handles) {
}

// static
bool ObjectTraits<sample::Bar>::DecodePointersAndHandles(
    sample::Bar* bar, const mojo::Message& message) {
  return true;
}

}  // namespace internal
}  // namespace mojo
