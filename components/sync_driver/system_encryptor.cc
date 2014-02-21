// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/system_encryptor.h"

#include "components/encryptor/encryptor.h"

namespace browser_sync {

SystemEncryptor::~SystemEncryptor() {}

bool SystemEncryptor::EncryptString(const std::string& plaintext,
                                    std::string* ciphertext) {
  return ::Encryptor::EncryptString(plaintext, ciphertext);
}

bool SystemEncryptor::DecryptString(const std::string& ciphertext,
                                    std::string* plaintext) {
  return ::Encryptor::DecryptString(ciphertext, plaintext);
}

}  // namespace browser_sync
