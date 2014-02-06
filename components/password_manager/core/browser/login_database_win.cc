// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/webdata/encryptor/encryptor.h"

LoginDatabase::EncryptionResult LoginDatabase::EncryptedString(
    const base::string16& plain_text,
    std::string* cipher_text) const {
  if (Encryptor::EncryptString16(plain_text, cipher_text))
    return ENCRYPTION_RESULT_SUCCESS;
  return ENCRYPTION_RESULT_ITEM_FAILURE;
}

LoginDatabase::EncryptionResult LoginDatabase::DecryptedString(
    const std::string& cipher_text,
    base::string16* plain_text) const {
  if (Encryptor::DecryptString16(cipher_text, plain_text))
    return ENCRYPTION_RESULT_SUCCESS;
  return ENCRYPTION_RESULT_ITEM_FAILURE;
}
