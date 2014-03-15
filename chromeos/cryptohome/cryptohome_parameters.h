// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_CRYPTOHOME_PARAMETERS_H_
#define CHROMEOS_CRYPTOHOME_CRYPTOHOME_PARAMETERS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace cryptohome {

enum AuthKeyPrivileges {
  PRIV_MOUNT = 1 << 0,              // Can mount with this key.
  PRIV_ADD = 1 << 1,                // Can add new keys.
  PRIV_REMOVE = 1 << 2,             // Can remove other keys.
  PRIV_MIGRATE = 1 << 3,            // Destroy all keys and replace with new.
  PRIV_AUTHORIZED_UPDATE = 1 << 4,  // Key can be updated in place.
  PRIV_DEFAULT = PRIV_MOUNT | PRIV_ADD | PRIV_REMOVE | PRIV_MIGRATE
};

// Identification of the user calling cryptohome method.
struct CHROMEOS_EXPORT Identification {
  explicit Identification(const std::string& user_id) : user_id(user_id) {}
  std::string user_id;
};

// Definition of the key (e.g. password) for the cryptohome.
// It contains authorization data along with extra parameters like perimissions
// associated with this key.
struct CHROMEOS_EXPORT KeyDefinition {
  KeyDefinition(const std::string& key,
                const std::string& label,
                int /*AuthKeyPrivileges*/ privileges);
  ~KeyDefinition();
  std::string label;

  int revision;
  std::string key;

  std::string encryption_key;
  std::string signature_key;
  // Privileges associated with key. Combination of |AuthKeyPrivileges| values.
  int privileges;
};

// Authorization attempt data for user.
struct CHROMEOS_EXPORT Authorization {
  Authorization(const std::string& key, const std::string& label);
  explicit Authorization(const KeyDefinition& key);
  std::string key;
  std::string label;
};

// Parameters for Mount call.
class CHROMEOS_EXPORT MountParameters {
 public:
  explicit MountParameters(bool ephemeral);
  ~MountParameters();

  // If |true|, the mounted home dir will be backed by tmpfs. If |false|, the
  // ephemeral users policy decides whether tmpfs or an encrypted directory is
  // used as the backend.
  bool ephemeral;

  // If not empty, home dir will be created with these keys if it exist.
  std::vector<KeyDefinition> create_keys;
};

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_CRYPTOHOME_PARAMETERS_H_
