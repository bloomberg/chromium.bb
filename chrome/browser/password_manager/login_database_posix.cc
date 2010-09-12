// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/login_database.h"

#include "base/utf_string_conversions.h"

// TODO: Actually encrypt passwords on Linux.

std::string LoginDatabase::EncryptedString(const string16& plain_text)
    const {
  return UTF16ToUTF8(plain_text);
}

string16 LoginDatabase::DecryptedString(const std::string& cipher_text)
    const {
  return UTF8ToUTF16(cipher_text);
}
