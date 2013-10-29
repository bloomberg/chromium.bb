// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/sample/generated/sample_foo_serialization.h"

#include <string.h>

#include "mojo/public/bindings/sample/generated/sample_bar_serialization.h"
#include "mojo/public/bindings/sample/generated/sample_foo.h"

namespace mojo {
namespace internal {

// static
size_t ObjectTraits<sample::Foo>::ComputeSizeOf(
    const sample::Foo* foo) {
  return sizeof(*foo) +
      mojo::internal::ComputeSizeOf(foo->bar()) +
      mojo::internal::ComputeSizeOf(foo->data()) +
      mojo::internal::ComputeSizeOf(foo->extra_bars()) +
      mojo::internal::ComputeSizeOf(foo->name()) +
      mojo::internal::ComputeSizeOf(foo->files());
}

// static
sample::Foo* ObjectTraits<sample::Foo>::Clone(
    const sample::Foo* foo, Buffer* buf) {
  sample::Foo* clone = sample::Foo::New(buf);
  memcpy(clone, foo, sizeof(*foo));

  clone->set_bar(mojo::internal::Clone(foo->bar(), buf));
  clone->set_data(mojo::internal::Clone(foo->data(), buf));
  clone->set_extra_bars(mojo::internal::Clone(foo->extra_bars(), buf));
  clone->set_name(mojo::internal::Clone(foo->name(), buf));
  clone->set_files(mojo::internal::Clone(foo->files(), buf));

  return clone;
}

// static
void ObjectTraits<sample::Foo>::EncodePointersAndHandles(
    sample::Foo* foo, std::vector<mojo::Handle>* handles) {
  Encode(&foo->bar_, handles);
  Encode(&foo->data_, handles);
  Encode(&foo->extra_bars_, handles);
  Encode(&foo->name_, handles);
  Encode(&foo->files_, handles);
}

// static
bool ObjectTraits<sample::Foo>::DecodePointersAndHandles(
    sample::Foo* foo, const mojo::Message& message) {
  if (!Decode(&foo->bar_, message))
    return false;
  if (!Decode(&foo->data_, message))
    return false;
  if (foo->_header_.num_fields >= 8) {
    if (!Decode(&foo->extra_bars_, message))
      return false;
  }
  if (foo->_header_.num_fields >= 9) {
    if (!Decode(&foo->name_, message))
      return false;
  }
  if (foo->_header_.num_fields >= 10) {
    if (!Decode(&foo->files_, message))
      return false;
  }

  // TODO: validate
  return true;
}

}  // namespace internal
}  // namespace mojo
