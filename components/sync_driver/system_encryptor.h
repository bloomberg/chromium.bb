// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYSTEM_ENCRYPTOR_H_
#define COMPONENTS_SYNC_DRIVER_SYSTEM_ENCRYPTOR_H_

#include "base/compiler_specific.h"
#include "sync/util/encryptor.h"

namespace sync_driver {

// Encryptor that uses the Chrome password manager's encryptor.
class SystemEncryptor : public syncer::Encryptor {
 public:
  virtual ~SystemEncryptor();

  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) OVERRIDE;

  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) OVERRIDE;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_SYSTEM_ENCRYPTOR_H_
