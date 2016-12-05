// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encryption_scheme.h"

namespace media {

EncryptionScheme::Pattern::Pattern() {}

EncryptionScheme::Pattern::Pattern(uint32_t encrypt_blocks,
                                   uint32_t skip_blocks)
    : encrypt_blocks_(encrypt_blocks), skip_blocks_(skip_blocks) {}

EncryptionScheme::Pattern::~Pattern() {}

uint32_t EncryptionScheme::Pattern::encrypt_blocks() const {
  return encrypt_blocks_;
}

uint32_t EncryptionScheme::Pattern::skip_blocks() const {
  return skip_blocks_;
}

bool EncryptionScheme::Pattern::Matches(const Pattern& other) const {
  return encrypt_blocks_ == other.encrypt_blocks() &&
         skip_blocks_ == other.skip_blocks();
}

bool EncryptionScheme::Pattern::IsInEffect() const {
  return encrypt_blocks_ != 0 && skip_blocks_ != 0;
}

EncryptionScheme::EncryptionScheme() {}

EncryptionScheme::EncryptionScheme(CipherMode mode, const Pattern& pattern)
    : mode_(mode), pattern_(pattern) {}

EncryptionScheme::~EncryptionScheme() {}

bool EncryptionScheme::is_encrypted() const {
  return mode_ != CIPHER_MODE_UNENCRYPTED;
}

EncryptionScheme::CipherMode EncryptionScheme::mode() const {
  return mode_;
}

const EncryptionScheme::Pattern& EncryptionScheme::pattern() const {
  return pattern_;
}

bool EncryptionScheme::Matches(const EncryptionScheme& other) const {
  return mode_ == other.mode_ && pattern_.Matches(other.pattern_);
}

}  // namespace media
