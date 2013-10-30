// Copyright $YEAR The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "$HEADER"

namespace $NAMESPACE {

// static
$CLASS* $CLASS::New(mojo::Buffer* buf) {
  return new (buf->Allocate(sizeof($CLASS))) $CLASS();
}

$CLASS::$CLASS() {
  _header_.num_bytes = sizeof(*this);
  _header_.num_fields = $NUM_FIELDS;
}

}  // namespace $NAMESPACE
