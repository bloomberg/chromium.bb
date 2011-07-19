// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ENCRYPTOR_H__
#define CHROME_BROWSER_PASSWORD_MANAGER_ENCRYPTOR_H__
#pragma once

#include <string>

#include "base/string16.h"

// The Encryptor class gives access to simple encryption and decryption of
// strings.  Note that on Mac, access to the system Keychain is required and
// these calls can block the current thread to collect user input.
class Encryptor {
 public:
  // Encrypt a string16. The output (second argument) is
  // really an array of bytes, but we're passing it back
  // as a std::string
  static bool EncryptString16(const string16& plaintext,
                              std::string* ciphertext);

  // Decrypt an array of bytes obtained with EncryptString16
  // back into a string16. Note that the input (first argument)
  // is a std::string, so you need to first get your (binary)
  // data into a string.
  static bool DecryptString16(const std::string& ciphertext,
                              string16* plaintext);

  // Encrypt a string.
  static bool EncryptString(const std::string& plaintext,
                            std::string* ciphertext);

  // Decrypt an array of bytes obtained with EnctryptString
  // back into a string. Note that the input (first argument)
  // is a std::string, so you need to first get your (binary)
  // data into a string.
  static bool DecryptString(const std::string& ciphertext,
                            std::string* plaintext);

#if defined(OS_MACOSX)
  // For unit testing purposes we instruct the Encryptor to use a mock Keychain
  // on the Mac.  The default is to use the real Keychain.
  static void UseMockKeychain(bool use_mock);
#endif

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Encryptor);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ENCRYPTOR_H__
