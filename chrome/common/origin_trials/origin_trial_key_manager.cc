// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/origin_trials/origin_trial_key_manager.h"

#include <stdint.h>

#include "base/base64.h"

// This is the default public key used for validating signatures.
// TODO(iclelland): Provide a mechanism to allow for multiple signing keys.
// https://crbug.com/584737
static const uint8_t kDefaultPublicKey[] = {
    0x7c, 0xc4, 0xb8, 0x9a, 0x93, 0xba, 0x6e, 0xe2, 0xd0, 0xfd, 0x03,
    0x1d, 0xfb, 0x32, 0x66, 0xc7, 0x3b, 0x72, 0xfd, 0x54, 0x3a, 0x07,
    0x51, 0x14, 0x66, 0xaa, 0x02, 0x53, 0x4e, 0x33, 0xa1, 0x15,
};

OriginTrialKeyManager::OriginTrialKeyManager()
    : public_key_(std::string(reinterpret_cast<const char*>(kDefaultPublicKey),
                              arraysize(kDefaultPublicKey))) {}

OriginTrialKeyManager::~OriginTrialKeyManager() {}

bool OriginTrialKeyManager::SetPublicKeyFromASCIIString(
    const std::string& ascii_public_key) {
  // Base64-decode the incoming string. Set the key if it is correctly formatted
  std::string new_public_key;
  if (!base::Base64Decode(ascii_public_key, &new_public_key))
    return false;
  if (new_public_key.size() != 32)
    return false;
  public_key_.swap(new_public_key);
  return true;
}

base::StringPiece OriginTrialKeyManager::GetPublicKey() const {
  return base::StringPiece(public_key_);
}
