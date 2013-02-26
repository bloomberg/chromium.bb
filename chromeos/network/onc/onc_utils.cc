// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_utils.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"

#define ONC_LOG_WARNING(message) NET_LOG_WARNING("ONC", message)
#define ONC_LOG_ERROR(message) NET_LOG_ERROR("ONC", message)

namespace chromeos {
namespace onc {

namespace {

const char kUnableToDecrypt[] = "Unable to decrypt encrypted ONC";
const char kUnableToDecode[] = "Unable to decode encrypted ONC";

}  // namespace

const char kEmptyUnencryptedConfiguration[] =
    "{\"Type\":\"UnencryptedConfiguration\",\"NetworkConfigurations\":[],"
    "\"Certificates\":[]}";

scoped_ptr<base::DictionaryValue> ReadDictionaryFromJson(
    const std::string& json) {
  std::string error;
  base::Value* root = base::JSONReader::ReadAndReturnError(
      json, base::JSON_ALLOW_TRAILING_COMMAS, NULL, &error);

  base::DictionaryValue* dict_ptr = NULL;
  if (!root || !root->GetAsDictionary(&dict_ptr)) {
    ONC_LOG_ERROR("Invalid JSON Dictionary: " + error);
    delete root;
  }

  return make_scoped_ptr(dict_ptr);
}

scoped_ptr<base::DictionaryValue> Decrypt(const std::string& passphrase,
                                          const base::DictionaryValue& root) {
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
      !root.GetString(toplevel_config::kType, &onc_type) ||
      onc_type != toplevel_config::kEncryptedConfiguration) {

    ONC_LOG_ERROR("Encrypted ONC malformed.");
    return scoped_ptr<base::DictionaryValue>();
  }

  if (hmac_method != encrypted::kSHA1 ||
      cipher != encrypted::kAES256 ||
      stretch_method != encrypted::kPBKDF2) {
    ONC_LOG_ERROR("Encrypted ONC unsupported encryption scheme.");
    return scoped_ptr<base::DictionaryValue>();
  }

  // Make sure iterations != 0, since that's not valid.
  if (iterations == 0) {
    ONC_LOG_ERROR(kUnableToDecrypt);
    return scoped_ptr<base::DictionaryValue>();
  }

  // Simply a sanity check to make sure we can't lock up the machine
  // for too long with a huge number (or a negative number).
  if (iterations < 0 || iterations > kMaxIterationCount) {
    ONC_LOG_ERROR("Too many iterations in encrypted ONC");
    return scoped_ptr<base::DictionaryValue>();
  }

  if (!base::Base64Decode(salt, &salt)) {
    ONC_LOG_ERROR(kUnableToDecode);
    return scoped_ptr<base::DictionaryValue>();
  }

  scoped_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::DeriveKeyFromPassword(crypto::SymmetricKey::AES,
                                                  passphrase,
                                                  salt,
                                                  iterations,
                                                  kKeySizeInBits));

  if (!base::Base64Decode(initial_vector, &initial_vector)) {
    ONC_LOG_ERROR(kUnableToDecode);
    return scoped_ptr<base::DictionaryValue>();
  }
  if (!base::Base64Decode(ciphertext, &ciphertext)) {
    ONC_LOG_ERROR(kUnableToDecode);
    return scoped_ptr<base::DictionaryValue>();
  }
  if (!base::Base64Decode(hmac, &hmac)) {
    ONC_LOG_ERROR(kUnableToDecode);
    return scoped_ptr<base::DictionaryValue>();
  }

  crypto::HMAC hmac_verifier(crypto::HMAC::SHA1);
  if (!hmac_verifier.Init(key.get()) ||
      !hmac_verifier.Verify(ciphertext, hmac)) {
    ONC_LOG_ERROR(kUnableToDecrypt);
    return scoped_ptr<base::DictionaryValue>();
  }

  crypto::Encryptor decryptor;
  if (!decryptor.Init(key.get(), crypto::Encryptor::CBC, initial_vector))  {
    ONC_LOG_ERROR(kUnableToDecrypt);
    return scoped_ptr<base::DictionaryValue>();
  }

  std::string plaintext;
  if (!decryptor.Decrypt(ciphertext, &plaintext)) {
    ONC_LOG_ERROR(kUnableToDecrypt);
    return scoped_ptr<base::DictionaryValue>();
  }

  scoped_ptr<base::DictionaryValue> new_root =
      ReadDictionaryFromJson(plaintext);
  if (new_root.get() == NULL) {
    ONC_LOG_ERROR("Property dictionary malformed.");
    return scoped_ptr<base::DictionaryValue>();
  }

  return new_root.Pass();
}

std::string GetSourceAsString(ONCSource source) {
  switch (source) {
    case ONC_SOURCE_DEVICE_POLICY:
      return "device policy";
    case ONC_SOURCE_USER_POLICY:
      return "user policy";
    case ONC_SOURCE_NONE:
      return "none";
    case ONC_SOURCE_USER_IMPORT:
      return "user import";
  }
  NOTREACHED() << "unknown ONC source " << source;
  return "unknown";
}

void ExpandField(const std::string fieldname,
                 const StringSubstitution& substitution,
                 base::DictionaryValue* onc_object) {
  std::string user_string;
  if (!onc_object->GetStringWithoutPathExpansion(fieldname, &user_string))
    return;

  std::string login_id;
  if (substitution.GetSubstitute(substitutes::kLoginIDField, &login_id)) {
    ReplaceSubstringsAfterOffset(&user_string, 0,
                                 onc::substitutes::kLoginIDField,
                                 login_id);
  }

  std::string email;
  if (substitution.GetSubstitute(substitutes::kEmailField, &email)) {
    ReplaceSubstringsAfterOffset(&user_string, 0,
                                 onc::substitutes::kEmailField,
                                 email);
  }

  onc_object->SetStringWithoutPathExpansion(fieldname, user_string);
}

void ExpandStringsInOncObject(
    const OncValueSignature& signature,
    const StringSubstitution& substitution,
    base::DictionaryValue* onc_object) {
  if (&signature == &kEAPSignature) {
    ExpandField(eap::kAnonymousIdentity, substitution, onc_object);
    ExpandField(eap::kIdentity, substitution, onc_object);
  } else if (&signature == &kL2TPSignature ||
             &signature == &kOpenVPNSignature) {
    ExpandField(vpn::kUsername, substitution, onc_object);
  }

  // Recurse into nested objects.
  for (base::DictionaryValue::Iterator it(*onc_object); !it.IsAtEnd();
       it.Advance()) {
    base::DictionaryValue* inner_object = NULL;
    if (!onc_object->GetDictionaryWithoutPathExpansion(it.key(), &inner_object))
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.key());

    ExpandStringsInOncObject(*field_signature->value_signature,
                             substitution, inner_object);
  }
}

}  // namespace onc
}  // namespace chromeos
