// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/login_database_mac.h"

std::string LoginDatabaseMac::EncryptedString(const string16& plain_text)
    const {
  return std::string();
}

string16 LoginDatabaseMac::DecryptedString(const std::string& cipher_text)
    const {
  return string16();
}
