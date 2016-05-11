// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/decrypt_context_impl.h"

#include <vector>

#include "chromecast/public/media/cast_decoder_buffer.h"

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
  output->resize(buffer->data_size());
  return Decrypt(buffer, output->data());
}

bool DecryptContextImpl::Decrypt(CastDecoderBuffer* buffer, uint8_t* output) {
  return false;
}

bool DecryptContextImpl::CanDecryptToBuffer() const {
  return false;
}

}  // namespace media
}  // namespace chromecast
