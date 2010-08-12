// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
#pragma once

#include <string>

#include "base/singleton.h"
#include "cros/chromeos_cryptohome.h"

namespace chromeos {

// This interface defines the interaction with the ChromeOS cryptohome library
// APIs.
class CryptohomeLibrary {
 public:
  virtual ~CryptohomeLibrary() {}

  // Asks cryptohomed to try to find the cryptohome for |user_email| and then
  // mount it using |passhash| to unlock the key.
  virtual bool Mount(const std::string& user_email,
                     const std::string& passhash,
                     int* error_code) = 0;

  // Asks cryptohomed to mount a tmpfs for BWSI mode.
  virtual bool MountForBwsi(int* error_code) = 0;

  // Asks cryptohomed to try to find the cryptohome for |user_email| and then
  // use |passhash| to unlock the key.
  virtual bool CheckKey(const std::string& user_email,
                        const std::string& passhash) = 0;

  // Asks cryptohomed to try to find the cryptohome for |user_email| and then
  // change from using |old_hash| to lock the key to using |new_hash|.
  virtual bool MigrateKey(const std::string& user_email,
                          const std::string& old_hash,
                          const std::string& new_hash) = 0;

  // Asks cryptohomed to try to find the cryptohome for |user_email| and then
  // nuke it.
  virtual bool Remove(const std::string& user_email) = 0;

  // Asks cryptohomed if a drive is currently mounted.
  virtual bool IsMounted() = 0;

  // Asks cryptohomed for the system salt.
  virtual CryptohomeBlob GetSystemSalt() = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static CryptohomeLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
