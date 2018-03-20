// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encryption_scheme.h"

namespace media {

EncryptionScheme::EncryptionScheme() = default;

EncryptionScheme::EncryptionScheme(CipherMode mode,
                                   const EncryptionPattern& pattern)
    : mode_(mode), pattern_(pattern) {}

EncryptionScheme::~EncryptionScheme() = default;

bool EncryptionScheme::is_encrypted() const {
  return mode_ != CIPHER_MODE_UNENCRYPTED;
}

EncryptionScheme::CipherMode EncryptionScheme::mode() const {
  return mode_;
}

const EncryptionPattern& EncryptionScheme::pattern() const {
  return pattern_;
}

bool EncryptionScheme::Matches(const EncryptionScheme& other) const {
  return mode_ == other.mode_ && pattern_.Matches(other.pattern_);
}

}  // namespace media
