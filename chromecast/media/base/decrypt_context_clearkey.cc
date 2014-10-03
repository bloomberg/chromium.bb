// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/decrypt_context_clearkey.h"

#include "base/logging.h"
#include "crypto/symmetric_key.h"

namespace chromecast {
namespace media {

DecryptContextClearKey::DecryptContextClearKey(crypto::SymmetricKey* key)
    : DecryptContext(KEY_SYSTEM_CLEAR_KEY),
      key_(key) {
  CHECK(key);
}

DecryptContextClearKey::~DecryptContextClearKey() {
}

crypto::SymmetricKey* DecryptContextClearKey::GetKey() const {
  return key_;
}

}  // namespace media
}  // namespace chromecast
