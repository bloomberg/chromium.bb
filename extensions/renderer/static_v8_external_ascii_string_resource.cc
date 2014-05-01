// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/static_v8_external_ascii_string_resource.h"

namespace extensions {

StaticV8ExternalAsciiStringResource::StaticV8ExternalAsciiStringResource(
    const base::StringPiece& buffer)
    : buffer_(buffer) {
}

StaticV8ExternalAsciiStringResource::~StaticV8ExternalAsciiStringResource() {
}

const char* StaticV8ExternalAsciiStringResource::data() const {
  return buffer_.data();
}

size_t StaticV8ExternalAsciiStringResource::length() const {
  return buffer_.length();
}

}  // namespace extensions
