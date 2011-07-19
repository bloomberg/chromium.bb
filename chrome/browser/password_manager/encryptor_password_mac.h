// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ENCRYPTOR_PASSWORD_H__
#define CHROME_BROWSER_PASSWORD_MANAGER_ENCRYPTOR_PASSWORD_H__
#pragma once

#include <string>

#include "base/basictypes.h"

class MacKeychain;

class EncryptorPassword {
 public:
  explicit EncryptorPassword(const MacKeychain& keychain)
      : keychain_(keychain) {
  }

  // Get the Encryptor password for this system.  If no password exists
  // in the Keychain then one is generated, stored in the Mac keychain, and
  // returned.
  // If one exists then it is fetched from the Keychain and returned.
  // If the user disallows access to the Keychain (or an error occurs) then an
  // empty string is returned.
  std::string GetEncryptorPassword() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(EncryptorPassword);
  const MacKeychain& keychain_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ENCRYPTOR_PASSWORD_H__
