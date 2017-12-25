// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_
#define CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_

#include <string>

#include "chromeos/chromeos_export.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {

// Creates an AuthorizationRequest from the given secret and label.
AuthorizationRequest CHROMEOS_EXPORT
CreateAuthorizationRequest(const std::string& label, const std::string& secret);

// Converts the given KeyDefinition to a Key.
void CHROMEOS_EXPORT KeyDefinitionToKey(const KeyDefinition& key_def, Key* key);

// Converts CryptohomeErrorCode to MountError.
CHROMEOS_EXPORT MountError
CryptohomeErrorToMountError(CryptohomeErrorCode code);

// Converts the given KeyAuthorizationData to AuthorizationData pointed to by
// |authorization_data|.
CHROMEOS_EXPORT
void KeyAuthorizationDataToAuthorizationData(
    const KeyAuthorizationData& authorization_data_proto,
    KeyDefinition::AuthorizationData* authorization_data);

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_
