// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/cryptohome_parameters.h"

#include <stddef.h>
#include <stdint.h>

#include "chromeos/dbus/cryptohome/key.pb.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/known_user.h"

namespace cryptohome {
namespace {

// Subsystem name for GaiaId migration status.
const char kCryptohome[] = "cryptohome";

const std::string GetCryptohomeId(const AccountId& account_id) {
  // Guest/kiosk/managed/public accounts have empty GaiaId. Default to email.
  if (account_id.GetAccountType() == AccountType::UNKNOWN)
    return account_id.GetUserEmail();  // Migrated

  if (GetGaiaIdMigrationStatus(account_id))
    return account_id.GetAccountIdKey();

  return account_id.GetUserEmail();  // Migrated
}

}  //  anonymous namespace

Identification::Identification() {}

Identification::Identification(const AccountId& account_id)
    : id_(GetCryptohomeId(account_id)) {}

Identification::Identification(const std::string& id) : id_(id) {}

Identification Identification::FromString(const std::string& id) {
  return Identification(id);
}

bool Identification::operator==(const Identification& other) const {
  return id_ == other.id_;
}

bool Identification::operator<(const Identification& right) const {
  return id_ < right.id_;
}

AccountId Identification::GetAccountId() const {
  const std::vector<AccountId> known_account_ids =
      user_manager::known_user::GetKnownAccountIds();

  // A LOT of tests start with --login_user <user>, and not registing this user
  // before. So we might have "known_user" entry without gaia_id.
  for (const AccountId& known_id : known_account_ids) {
    if (known_id.HasAccountIdKey() && known_id.GetAccountIdKey() == id_) {
      return known_id;
    }
  }

  for (const AccountId& known_id : known_account_ids) {
    if (known_id.GetUserEmail() == id_) {
      return known_id;
    }
  }

  return user_manager::known_user::GetAccountId(id_, std::string() /* id */,
                                                AccountType::UNKNOWN);
}

KeyDefinition::AuthorizationData::Secret::Secret() : encrypt(false),
                                                     sign(false),
                                                     wrapped(false) {
}

KeyDefinition::AuthorizationData::Secret::Secret(
    bool encrypt,
    bool sign,
    const std::string& symmetric_key,
    const std::string& public_key,
    bool wrapped)
    : encrypt(encrypt),
      sign(sign),
      symmetric_key(symmetric_key),
      public_key(public_key),
      wrapped(wrapped) {
}

bool KeyDefinition::AuthorizationData::Secret::operator==(
    const Secret& other) const {
  return encrypt == other.encrypt &&
         sign == other.sign &&
         symmetric_key == other.symmetric_key &&
         public_key == other.public_key &&
         wrapped == other.wrapped;
}

KeyDefinition::AuthorizationData::AuthorizationData() : type(TYPE_HMACSHA256) {
}

KeyDefinition::AuthorizationData::AuthorizationData(
    bool encrypt,
    bool sign,
    const std::string& symmetric_key) : type(TYPE_HMACSHA256) {
    secrets.push_back(Secret(encrypt,
                             sign,
                             symmetric_key,
                             std::string() /* public_key */,
                             false /* wrapped */));
}

KeyDefinition::AuthorizationData::AuthorizationData(
    const AuthorizationData& other) = default;

KeyDefinition::AuthorizationData::~AuthorizationData() {
}

bool KeyDefinition::AuthorizationData::operator==(
    const AuthorizationData& other) const {
  if (type != other.type || secrets.size() != other.secrets.size())
    return false;
  for (size_t i = 0; i < secrets.size(); ++i) {
    if (!(secrets[i] == other.secrets[i]))
      return false;
  }
  return true;
}

KeyDefinition::ProviderData::ProviderData() {
}

KeyDefinition::ProviderData::ProviderData(const std::string& name)
    : name(name) {
}

KeyDefinition::ProviderData::ProviderData(const ProviderData& other)
    : name(other.name) {
  if (other.number)
    number.reset(new int64_t(*other.number));
  if (other.bytes)
    bytes.reset(new std::string(*other.bytes));
}

KeyDefinition::ProviderData::ProviderData(const std::string& name,
                                          int64_t number)
    : name(name), number(new int64_t(number)) {}

KeyDefinition::ProviderData::ProviderData(const std::string& name,
                                          const std::string& bytes)
    : name(name),
      bytes(new std::string(bytes)) {
}

void KeyDefinition::ProviderData::operator=(const ProviderData& other) {
  name = other.name;
  number.reset(other.number ? new int64_t(*other.number) : NULL);
  bytes.reset(other.bytes ? new std::string(*other.bytes) : NULL);
}

KeyDefinition::ProviderData::~ProviderData() {
}

bool KeyDefinition::ProviderData::operator==(const ProviderData& other) const {
  const bool has_number = number != nullptr;
  const bool other_has_number = other.number != nullptr;
  const bool has_bytes = bytes != nullptr;
  const bool other_has_bytes = other.bytes != nullptr;
  return name == other.name &&
         has_number == other_has_number &&
         has_bytes == other_has_bytes &&
         (!has_number || (*number == *other.number)) &&
         (!has_bytes || (*bytes == *other.bytes));
}

KeyDefinition::KeyDefinition() : type(TYPE_PASSWORD),
                                 privileges(0),
                                 revision(0) {
}

KeyDefinition::KeyDefinition(const std::string& secret,
                             const std::string& label,
                             int /*AuthKeyPrivileges*/ privileges)
    : type(TYPE_PASSWORD),
      label(label),
      privileges(privileges),
      revision(0),
      secret(secret) {
}

KeyDefinition::KeyDefinition(const KeyDefinition& other) = default;

KeyDefinition::~KeyDefinition() {
}

bool KeyDefinition::operator==(const KeyDefinition& other) const {
  if (type != other.type ||
      label != other.label ||
      privileges != other.privileges ||
      revision != other.revision ||
      authorization_data.size() != other.authorization_data.size() ||
      provider_data.size() != other.provider_data.size()) {
    return false;
  }

  for (size_t i = 0; i < authorization_data.size(); ++i) {
    if (!(authorization_data[i] == other.authorization_data[i]))
      return false;
  }
  for (size_t i = 0; i < provider_data.size(); ++i) {
    if (!(provider_data[i] == other.provider_data[i]))
      return false;
  }
  return true;
}

Authorization::Authorization(const std::string& key, const std::string& label)
    : key(key),
      label(label) {
}

Authorization::Authorization(const KeyDefinition& key_def)
    : key(key_def.secret),
      label(key_def.label) {
}

bool Authorization::operator==(const Authorization& other) const {
  return key == other.key && label == other.label;
}

MountParameters::MountParameters(bool ephemeral) : ephemeral(ephemeral) {
}

MountParameters::MountParameters(const MountParameters& other) = default;

bool MountParameters::operator==(const MountParameters& other) const {
  return ephemeral == other.ephemeral && create_keys == other.create_keys;
}

MountParameters::~MountParameters() {
}

bool GetGaiaIdMigrationStatus(const AccountId& account_id) {
  return user_manager::known_user::GetGaiaIdMigrationStatus(account_id,
                                                            kCryptohome);
}

void SetGaiaIdMigrationStatusDone(const AccountId& account_id) {
  user_manager::known_user::SetGaiaIdMigrationStatusDone(account_id,
                                                         kCryptohome);
}

}  // namespace cryptohome

namespace BASE_HASH_NAMESPACE {

std::size_t hash<cryptohome::Identification>::operator()(
    const cryptohome::Identification& cryptohome_id) const {
  return hash<std::string>()(cryptohome_id.id());
}

}  // namespace BASE_HASH_NAMESPACE
