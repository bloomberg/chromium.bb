// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/supervised_user_authentication.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chromeos/chromeos_switches.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"

namespace chromeos {

namespace {

// Byte size of hash salt.
const unsigned kSaltSize = 32;

// Parameters of cryptographic hashing.
const unsigned kNumIterations = 1234;
const unsigned kKeySizeInBits = 256;

std::string CreateSalt() {
    char result[kSaltSize];
    crypto::RandBytes(&result, sizeof(result));
    return StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(result),
        sizeof(result)));
}

std::string BuildPasswordForHashWithSaltSchema(
  const std::string& salt,
  const std::string& plain_password) {
  scoped_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::DeriveKeyFromPassword(
          crypto::SymmetricKey::AES,
          plain_password, salt,
          kNumIterations, kKeySizeInBits));
  std::string raw_result, result;
  key->GetRawKey(&raw_result);
  base::Base64Encode(raw_result, &result);
  return result;
}

}  // namespace

SupervisedUserAuthentication::SupervisedUserAuthentication(
    SupervisedUserManager* owner)
      : owner_(owner),
        migration_enabled_(false),
        stable_schema_(SCHEMA_PLAIN) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSupervisedPasswordSync)) {
    migration_enabled_ = true;
    stable_schema_ = SCHEMA_SALT_HASHED;
  }
}

SupervisedUserAuthentication::~SupervisedUserAuthentication() {}

std::string SupervisedUserAuthentication::TransformPassword(
    const std::string& user_id,
    const std::string& password) {
  int user_schema = GetPasswordSchema(user_id);
  if (user_schema == SCHEMA_PLAIN)
    return password;

  if (user_schema == SCHEMA_SALT_HASHED) {
    base::DictionaryValue holder;
    std::string salt;
    owner_->GetPasswordInformation(user_id, &holder);
    holder.GetStringWithoutPathExpansion(kSalt, &salt);
    DCHECK(!salt.empty());
    return BuildPasswordForHashWithSaltSchema(salt, password);
  }
  NOTREACHED();
  return password;
}

bool SupervisedUserAuthentication::FillDataForNewUser(
    const std::string& user_id,
    const std::string& password,
    base::DictionaryValue* password_data) {
  Schema schema = stable_schema_;
  if (schema == SCHEMA_PLAIN)
    return false;
  if (schema == SCHEMA_SALT_HASHED) {
    password_data->SetIntegerWithoutPathExpansion(kSchemaVersion, schema);
    std::string salt = CreateSalt();
    password_data->SetStringWithoutPathExpansion(kSalt, salt);
    password_data->SetIntegerWithoutPathExpansion(kPasswordRevision,
        kMinPasswordRevision);
    password_data->SetStringWithoutPathExpansion(kEncryptedPassword,
        BuildPasswordForHashWithSaltSchema(salt, password));
    return true;
  }
  NOTREACHED();
  return false;
}

void SupervisedUserAuthentication::StorePasswordData(
    const std::string& user_id,
    const base::DictionaryValue& password_data) {
  DictionaryValue holder;
  owner_->GetPasswordInformation(user_id, &holder);
  const base::Value* value;
  if (password_data.GetWithoutPathExpansion(kSchemaVersion, &value))
      holder.SetWithoutPathExpansion(kSchemaVersion, value->DeepCopy());
  if (password_data.GetWithoutPathExpansion(kSalt, &value))
      holder.SetWithoutPathExpansion(kSalt, value->DeepCopy());
  if (password_data.GetWithoutPathExpansion(kPasswordRevision, &value))
      holder.SetWithoutPathExpansion(kPasswordRevision, value->DeepCopy());
  owner_->SetPasswordInformation(user_id, &holder);
}

bool SupervisedUserAuthentication::PasswordNeedsMigration(
    const std::string& user_id) {
  return GetPasswordSchema(user_id) < stable_schema_;
}

SupervisedUserAuthentication::Schema
SupervisedUserAuthentication::GetPasswordSchema(
  const std::string& user_id) {
  base::DictionaryValue holder;

  owner_->GetPasswordInformation(user_id, &holder);
  // Default version.
  int schema_version_index;
  Schema schema_version = SCHEMA_PLAIN;
  if (holder.GetIntegerWithoutPathExpansion(kSchemaVersion,
                                            &schema_version_index)) {
    schema_version = static_cast<Schema>(schema_version_index);
  }
  return schema_version;
}

void SupervisedUserAuthentication::SchedulePasswordMigration(
    const std::string& supervised_user_id,
    const std::string& user_password,
    SupervisedUserLoginFlow* user_flow) {
  // TODO(antrim): Add actual migration code once cryptohome has required API.
}

}  // namespace chromeos
