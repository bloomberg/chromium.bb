// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_CHROME_ENCRYPTOR_H_
#define CHROME_BROWSER_SYNC_GLUE_CHROME_ENCRYPTOR_H_
#pragma once

#include "base/compiler_specific.h"
#include "sync/util/encryptor.h"

namespace browser_sync {

// Encryptor that uses the Chrome password manager's encryptor.
class ChromeEncryptor : public csync::Encryptor {
 public:
  virtual ~ChromeEncryptor();

  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) OVERRIDE;

  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) OVERRIDE;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_CHROME_ENCRYPTOR_H_
