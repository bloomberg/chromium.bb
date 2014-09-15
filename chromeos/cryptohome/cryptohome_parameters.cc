// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/cryptohome_parameters.h"

#include "chromeos/dbus/cryptohome/key.pb.h"

namespace cryptohome {

Identification::Identification(const std::string& user_id) : user_id(user_id) {
}

bool Identification::operator==(const Identification& other) const {
  return user_id == other.user_id;
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
    number.reset(new int64(*other.number));
  if (other.bytes)
    bytes.reset(new std::string(*other.bytes));
}

KeyDefinition::ProviderData::ProviderData(const std::string& name, int64 number)
    : name(name),
      number(new int64(number)) {
}

KeyDefinition::ProviderData::ProviderData(const std::string& name,
                                          const std::string& bytes)
    : name(name),
      bytes(new std::string(bytes)) {
}

void KeyDefinition::ProviderData::operator=(const ProviderData& other) {
  name = other.name;
  number.reset(other.number ? new int64(*other.number) : NULL);
  bytes.reset(other.bytes ? new std::string(*other.bytes) : NULL);
}

KeyDefinition::ProviderData::~ProviderData() {
}

bool KeyDefinition::ProviderData::operator==(const ProviderData& other) const {
  const bool has_number = number;
  const bool other_has_number = other.number;
  const bool has_bytes = bytes;
  const bool other_has_bytes = other.bytes;
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

bool MountParameters::operator==(const MountParameters& other) const {
  return ephemeral == other.ephemeral && create_keys == other.create_keys;
}

MountParameters::~MountParameters() {
}

}  // namespace cryptohome
