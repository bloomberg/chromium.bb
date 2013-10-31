// Copyright $YEAR The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "$SERIALIZATION_HEADER"

#include <string.h>

#include "$CLASS_HEADER"

namespace mojo {
namespace internal {

// static
size_t ObjectTraits<$FULL_CLASS>::ComputeSizeOf(
    const $FULL_CLASS* $NAME) {
$SIZES
}

// static
$FULL_CLASS* ObjectTraits<$FULL_CLASS>::Clone(
    const $FULL_CLASS* $NAME, Buffer* buf) {
  $FULL_CLASS* clone = $FULL_CLASS::New(buf);
  memcpy(clone, $NAME, sizeof(*$NAME));

$CLONES

  return clone;
}

// static
void ObjectTraits<$FULL_CLASS>::EncodePointersAndHandles(
    $FULL_CLASS* $NAME, std::vector<mojo::Handle>* handles) {
$ENCODES
}

// static
bool ObjectTraits<$FULL_CLASS>::DecodePointersAndHandles(
    $FULL_CLASS* $NAME, const mojo::Message& message) {
$DECODES
  return true;
}

}  // namespace internal
}  // namespace mojo
