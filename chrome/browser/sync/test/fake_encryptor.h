// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_FAKE_ENCRYPTOR_H_
#define CHROME_BROWSER_SYNC_TEST_FAKE_ENCRYPTOR_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/sync/util/encryptor.h"

namespace browser_sync {

// Encryptor which simply base64-encodes the plaintext to get the
// ciphertext.  Obviously, this should be used only for testing.
class FakeEncryptor : public Encryptor {
 public:
  virtual ~FakeEncryptor();

  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) OVERRIDE;

  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) OVERRIDE;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_TEST_FAKE_ENCRYPTOR_H_
