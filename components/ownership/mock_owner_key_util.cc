// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/mock_owner_key_util.h"

#include "base/files/file_path.h"
#include "crypto/rsa_private_key.h"

namespace ownership {

MockOwnerKeyUtil::MockOwnerKeyUtil() {
}

MockOwnerKeyUtil::~MockOwnerKeyUtil() {
}

bool MockOwnerKeyUtil::ImportPublicKey(std::vector<uint8>* output) {
  *output = public_key_;
  return !public_key_.empty();
}

#if defined(USE_NSS)
crypto::RSAPrivateKey* MockOwnerKeyUtil::FindPrivateKeyInSlot(
    const std::vector<uint8>& key,
    PK11SlotInfo* slot) {
  return private_key_.get() ? private_key_->Copy() : NULL;
}
#endif  // defined(USE_NSS)

bool MockOwnerKeyUtil::IsPublicKeyPresent() {
  return !public_key_.empty();
}

void MockOwnerKeyUtil::Clear() {
  public_key_.clear();
  private_key_.reset();
}

void MockOwnerKeyUtil::SetPublicKey(const std::vector<uint8>& key) {
  public_key_ = key;
}

void MockOwnerKeyUtil::SetPublicKeyFromPrivateKey(
    const crypto::RSAPrivateKey& key) {
  key.ExportPublicKey(&public_key_);
}

void MockOwnerKeyUtil::SetPrivateKey(scoped_ptr<crypto::RSAPrivateKey> key) {
  private_key_ = key.Pass();
  private_key_->ExportPublicKey(&public_key_);
}

}  // namespace ownership
