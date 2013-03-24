// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_encryptor.h"

#include "components/webdata/encryptor/encryptor.h"

namespace browser_sync {

ChromeEncryptor::~ChromeEncryptor() {}

bool ChromeEncryptor::EncryptString(const std::string& plaintext,
                                    std::string* ciphertext) {
  return ::Encryptor::EncryptString(plaintext, ciphertext);
}

bool ChromeEncryptor::DecryptString(const std::string& ciphertext,
                                    std::string* plaintext) {
  return ::Encryptor::DecryptString(ciphertext, plaintext);
}

}  // namespace browser_sync
