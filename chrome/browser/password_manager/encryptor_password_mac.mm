// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/encryptor_password_mac.h"

#import <Security/Security.h>

#include "chrome/browser/keychain_mac.h"
#include "chrome/browser/sync/util/crypto_helpers.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Generates a random password and adds it to the Keychain.  The added password
// is returned from the function.  If an error occurs, an empty password is
// returned.
std::string AddRandomPasswordToKeychain(const MacKeychain& keychain,
                                        const std::string& service_name,
                                        const std::string& account_name) {
  std::string password = Generate128BitRandomHexString();
  void* password_data =
      const_cast<void*>(static_cast<const void*>(password.data()));

  OSStatus error = keychain.AddGenericPassword(NULL,
                                               service_name.size(),
                                               service_name.data(),
                                               account_name.size(),
                                               account_name.data(),
                                               password.size(),
                                               password_data,
                                               NULL);

  if (error != noErr) {
    DLOG(ERROR) << "Keychain add failed with error " << error;
    return std::string();
  }

  return password;
}

}  // namespace

std::string EncryptorPassword::GetEncryptorPassword() const {
  // These two strings ARE indeed user facing.  But they are used to access
  // the encryption keyword.  So as to not lose encrypted data when system
  // locale changes we DO NOT LOCALIZE.
  const std::string service_name = "Chrome Safe Storage";
  const std::string account_name = "Chrome";

  UInt32 password_length = 0;
  void* password_data = NULL;
  OSStatus error = keychain_.FindGenericPassword(NULL,
                                                 service_name.size(),
                                                 service_name.data(),
                                                 account_name.size(),
                                                 account_name.data(),
                                                 &password_length,
                                                 &password_data,
                                                 NULL);

  if (error == noErr) {
    std::string password =
        std::string(static_cast<char*>(password_data), password_length);
    keychain_.ItemFreeContent(NULL, password_data);
    return password;
  } else if (error == errSecItemNotFound) {
    return AddRandomPasswordToKeychain(keychain_, service_name, account_name);
  } else {
    DLOG(ERROR) << "Keychain lookup failed with error " << error;
    return std::string();
  }
}


