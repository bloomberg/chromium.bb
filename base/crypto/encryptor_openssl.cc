// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/encryptor.h"

#include "base/logging.h"

namespace base {

Encryptor::Encryptor() {
}

Encryptor::~Encryptor() {
}

bool Encryptor::Init(SymmetricKey* key, Mode mode, const std::string& iv) {
  NOTIMPLEMENTED();
  return false;
}

bool Encryptor::Encrypt(const std::string& plaintext, std::string* ciphertext) {
  NOTIMPLEMENTED();
  return false;
}

bool Encryptor::Decrypt(const std::string& ciphertext, std::string* plaintext) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace base
