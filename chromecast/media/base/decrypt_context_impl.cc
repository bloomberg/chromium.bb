// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/decrypt_context_impl.h"

namespace chromecast {
namespace media {

DecryptContextImpl::DecryptContextImpl(CastKeySystem key_system)
    : key_system_(key_system) {}

DecryptContextImpl::~DecryptContextImpl() {}

CastKeySystem DecryptContextImpl::GetKeySystem() {
  return key_system_;
}

bool DecryptContextImpl::Decrypt(CastDecoderBuffer* buffer,
                                 std::vector<uint8_t>* output) {
  return false;
}

crypto::SymmetricKey* DecryptContextImpl::GetKey() const {
  return NULL;
}

}  // namespace media
}  // namespace chromecast
