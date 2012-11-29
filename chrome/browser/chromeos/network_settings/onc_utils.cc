// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_settings/onc_utils.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/onc_constants.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace onc {

scoped_ptr<base::DictionaryValue> ReadDictionaryFromJson(
    const std::string& json,
    std::string* error) {
  base::Value* root = base::JSONReader::ReadAndReturnError(
      json, base::JSON_ALLOW_TRAILING_COMMAS, NULL, error);

  base::DictionaryValue* dict_ptr = NULL;
  if (root != NULL && !root->GetAsDictionary(&dict_ptr)) {
    if (error) {
      *error = l10n_util::GetStringUTF8(
          IDS_NETWORK_CONFIG_ERROR_NETWORK_NOT_A_JSON_DICTIONARY);
    }
    delete root;
  }

  return make_scoped_ptr(dict_ptr);
}

scoped_ptr<base::DictionaryValue> Decrypt(const std::string& passphrase,
                                          const base::DictionaryValue& root,
                                          std::string* error) {
  const int kKeySizeInBits = 256;
  const int kMaxIterationCount = 500000;
  std::string onc_type;
  std::string initial_vector;
  std::string salt;
  std::string cipher;
  std::string stretch_method;
  std::string hmac_method;
  std::string hmac;
  int iterations;
  std::string ciphertext;

  if (!root.GetString(encrypted::kCiphertext, &ciphertext) ||
      !root.GetString(encrypted::kCipher, &cipher) ||
      !root.GetString(encrypted::kHMAC, &hmac) ||
      !root.GetString(encrypted::kHMACMethod, &hmac_method) ||
      !root.GetString(encrypted::kIV, &initial_vector) ||
      !root.GetInteger(encrypted::kIterations, &iterations) ||
      !root.GetString(encrypted::kSalt, &salt) ||
      !root.GetString(encrypted::kStretch, &stretch_method) ||
      !root.GetString(encrypted::kType, &onc_type) ||
      onc_type != kEncryptedConfiguration) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_MALFORMED);
    return scoped_ptr<base::DictionaryValue>();
  }

  if (hmac_method != encrypted::kSHA1 ||
      cipher != encrypted::kAES256 ||
      stretch_method != encrypted::kPBKDF2) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_UNSUPPORTED_ENCRYPTION);
    return scoped_ptr<base::DictionaryValue>();
  }

  // Make sure iterations != 0, since that's not valid.
  if (iterations == 0) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_UNABLE_TO_DECRYPT);
    return scoped_ptr<base::DictionaryValue>();
  }

  // Simply a sanity check to make sure we can't lock up the machine
  // for too long with a huge number (or a negative number).
  if (iterations < 0 || iterations > kMaxIterationCount) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_TOO_MANY_ITERATIONS);
    return scoped_ptr<base::DictionaryValue>();
  }

  if (!base::Base64Decode(salt, &salt)) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_UNABLE_TO_DECODE);
    return scoped_ptr<base::DictionaryValue>();
  }

  scoped_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::DeriveKeyFromPassword(crypto::SymmetricKey::AES,
                                                  passphrase,
                                                  salt,
                                                  iterations,
                                                  kKeySizeInBits));

  if (!base::Base64Decode(initial_vector, &initial_vector)) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_UNABLE_TO_DECODE);
    return scoped_ptr<base::DictionaryValue>();
  }
  if (!base::Base64Decode(ciphertext, &ciphertext)) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_UNABLE_TO_DECODE);
    return scoped_ptr<base::DictionaryValue>();
  }
  if (!base::Base64Decode(hmac, &hmac)) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_UNABLE_TO_DECODE);
    return scoped_ptr<base::DictionaryValue>();
  }

  crypto::HMAC hmac_verifier(crypto::HMAC::SHA1);
  if (!hmac_verifier.Init(key.get()) ||
      !hmac_verifier.Verify(ciphertext, hmac)) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_UNABLE_TO_DECRYPT);
    return scoped_ptr<base::DictionaryValue>();
  }

  crypto::Encryptor decryptor;
  if (!decryptor.Init(key.get(), crypto::Encryptor::CBC, initial_vector))  {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_UNABLE_TO_DECRYPT);
    return scoped_ptr<base::DictionaryValue>();
  }

  std::string plaintext;
  if (!decryptor.Decrypt(ciphertext, &plaintext)) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_ENCRYPTED_ONC_UNABLE_TO_DECRYPT);
    return scoped_ptr<base::DictionaryValue>();
  }

  scoped_ptr<base::DictionaryValue> new_root =
      ReadDictionaryFromJson(plaintext, error);
  if (new_root.get() == NULL && error->empty()) {
    *error = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_NETWORK_PROP_DICT_MALFORMED);
  }
  return new_root.Pass();
}

}  // chromeos
}  // onc
