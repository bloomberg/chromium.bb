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

KeyDefinition::KeyDefinition(const std::string& key,
                             const std::string& label,
                             int /*AuthKeyPrivileges*/ privileges)
    : label(label),
      revision(1),
      key(key),
      privileges(privileges) {
}

KeyDefinition::~KeyDefinition() {
}

bool KeyDefinition::operator==(const KeyDefinition& other) const {
  return label == other.label &&
         revision == other.revision &&
         key == other.key &&
         encryption_key == other.encryption_key &&
         signature_key == other.signature_key &&
         privileges == other.privileges;
}

Authorization::Authorization(const std::string& key, const std::string& label)
    : key(key),
      label(label) {
}

Authorization::Authorization(const KeyDefinition& key_def)
    : key(key_def.key),
      label(key_def.label) {
}

bool Authorization::operator==(const Authorization& other) const {
  return key == other.key && label == other.label;
}

RetrievedKeyData::ProviderData::ProviderData(const std::string& name)
    : name(name) {
}

RetrievedKeyData::ProviderData::~ProviderData() {
}

RetrievedKeyData::RetrievedKeyData(Type type,
                                   const std::string& label,
                                   int64 revision) : type(type),
                                                     label(label),
                                                     privileges(0),
                                                     revision(revision) {
}

RetrievedKeyData::~RetrievedKeyData() {
}

MountParameters::MountParameters(bool ephemeral) : ephemeral(ephemeral) {
}

bool MountParameters::operator==(const MountParameters& other) const {
  return ephemeral == other.ephemeral && create_keys == other.create_keys;
}

MountParameters::~MountParameters() {
}

}  // namespace cryptohome
