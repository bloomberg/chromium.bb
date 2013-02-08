// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_PASSPHRASE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_PASSPHRASE_H_

#include <string>

#include "base/basictypes.h"

// This class can be used to derive a hash of a provided passphrase. When an
// empty salt is given as parameter to the constructor, a new salt is
// generated randomly, which can be accessed through the |GetSalt| method.
class ManagedUserPassphrase {
 public:
  explicit ManagedUserPassphrase(const std::string& salt);
  ~ManagedUserPassphrase();

  std::string GetSalt();

  // This function generates a hash from a passphrase, which should be provided
  // as the first parameter. On return, the second parameter will contain the
  // Base64-encoded hash of the password.
  bool GenerateHashFromPassphrase(const std::string& passphrase,
                                  std::string* encoded_passphrase_hash) const;

 private:
  bool GetPassphraseHash(const std::string& passphrase,
                         std::string* passphrase_hash) const;
  void GenerateRandomSalt();

  std::string salt_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserPassphrase);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_PASSPHRASE_H_
