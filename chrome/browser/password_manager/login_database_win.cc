// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/password_manager/login_database.h"

std::string LoginDatabase::EncryptedString(const string16& plain_text)
    const {
  std::string cipher_text;
  if (!Encryptor::EncryptString16(plain_text, &cipher_text))
    NOTREACHED() << "Failed to encrypt string";
  return cipher_text;
}

string16 LoginDatabase::DecryptedString(const std::string& cipher_text)
    const {
  string16 plain_text;
  if (!Encryptor::DecryptString16(cipher_text, &plain_text))
    NOTREACHED() << "Failed to decrypt string";
  return plain_text;
}
