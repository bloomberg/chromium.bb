// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/encryptor.h"

namespace base {

// TODO(albertb): Implement on Mac using the Common Crypto Library:
// http://developer.apple.com/mac/library/documentation/Darwin/Reference/ManPages/man3/CCCryptor.3cc.html#//apple_ref/doc/man/10.5/3cc/CCCryptor?useVersion=10.5

Encryptor::Encryptor() {
}

Encryptor::~Encryptor() {
}

bool Encryptor::Init(SymmetricKey* key, Mode mode, const std::string& iv) {
  return false;
}

bool Encryptor::Encrypt(const std::string& plaintext, std::string* ciphertext) {
  return false;
}

bool Encryptor::Decrypt(const std::string& ciphertext, std::string* plaintext) {
  return false;
}

}  // namespace base
