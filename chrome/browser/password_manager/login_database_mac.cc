// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/login_database.h"

// On the Mac, the LoginDatabase nulls out passwords, so that we can use the
// rest of the database as a suplemental storage system to complement Keychain,
// providing storage of fields Keychain doesn't allow.

std::string LoginDatabase::EncryptedString(const string16& plain_text) const {
  return std::string();
}

string16 LoginDatabase::DecryptedString(const std::string& cipher_text) const {
  return string16();
}
