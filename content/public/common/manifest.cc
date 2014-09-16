// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/manifest.h"

namespace content {

const size_t Manifest::kMaxIPCStringLength = 4 * 1024;

Manifest::Manifest() {
}

Manifest::~Manifest() {
}

bool Manifest::IsEmpty() const {
  return name.is_null() && short_name.is_null();
}

} // namespace content
