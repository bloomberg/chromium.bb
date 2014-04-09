// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

// On the Mac, the LoginDatabase nulls out passwords, so that we can use the
// rest of the database as a suplemental storage system to complement Keychain,
// providing storage of fields Keychain doesn't allow.

namespace password_manager {

LoginDatabase::EncryptionResult LoginDatabase::EncryptedString(
    const base::string16& plain_text,
    std::string* cipher_text) const {
  *cipher_text = std::string();
  return ENCRYPTION_RESULT_SUCCESS;
}

LoginDatabase::EncryptionResult LoginDatabase::DecryptedString(
    const std::string& cipher_text,
    base::string16* plain_text) const {
  *plain_text = base::string16();
  return ENCRYPTION_RESULT_SUCCESS;
}

}  // namespace password_manager
