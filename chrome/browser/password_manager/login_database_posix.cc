// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/password_manager/login_database.h"

// TODO: Actually encrypt passwords on Linux.

std::string LoginDatabase::EncryptedString(const string16& plain_text)
    const {
  return UTF16ToASCII(plain_text);
}

string16 LoginDatabase::DecryptedString(const std::string& cipher_text)
    const {
  return ASCIIToUTF16(cipher_text);
}
