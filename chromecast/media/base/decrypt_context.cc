// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/decrypt_context.h"

namespace chromecast {
namespace media {

DecryptContext::DecryptContext(CastKeySystem key_system)
    : key_system_(key_system) {
}

DecryptContext::~DecryptContext() {
}

crypto::SymmetricKey* DecryptContext::GetKey() const {
  return NULL;
}

}  // namespace media
}  // namespace chromecast
