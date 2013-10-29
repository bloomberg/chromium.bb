// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/sample/generated/sample_bar.h"

namespace sample {

// static
Bar* Bar::New(mojo::Buffer* buf) {
  return new (buf->Allocate(sizeof(Bar))) Bar();
}

Bar::Bar() {
  _header_.num_bytes = sizeof(*this);
  _header_.num_fields = 3;
}

}  // namespace sample
