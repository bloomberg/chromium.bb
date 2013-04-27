// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ATTESTATION_ATTESTATION_CONSTANTS_H_
#define CHROMEOS_ATTESTATION_ATTESTATION_CONSTANTS_H_

#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace attestation {

// Options available for customizing an attestation certificate.
enum AttestationCertificateOptions {
  CERTIFICATE_OPTION_NONE = 0,
  // A stable identifier is simply an identifier that is not affected by device
  // state changes, including device recovery.
  CERTIFICATE_INCLUDE_STABLE_ID = 1,
  // Device state information contains a quoted assertion of whether the device
  // is in verified mode.
  CERTIFICATE_INCLUDE_DEVICE_STATE = 1 << 1,
};

// Key types supported by the Chrome OS attestation subsystem.
enum AttestationKeyType {
  // The key will be associated with the device itself and will be available
  // regardless of which user is signed-in.
  KEY_DEVICE,
  // The key will be associated with the current user and will only be available
  // when that user is signed-in.
  KEY_USER,
};

// Options available for customizing an attestation challenge response.
enum AttestationChallengeOptions {
  CHALLENGE_OPTION_NONE = 0,
  // Indicates that a SignedPublicKeyAndChallenge should be embedded in the
  // challenge response.
  CHALLENGE_INCLUDE_SIGNED_PUBLIC_KEY = 1,
};

// Available attestation certificate profiles.
enum AttestationCertificateProfile {
  // Uses the following certificate options:
  //   CERTIFICATE_INCLUDE_STABLE_ID
  //   CERTIFICATE_INCLUDE_DEVICE_STATE
  PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
  // Uses the following certificate options:
  //   CERTIFICATE_INCLUDE_DEVICE_STATE
  PROFILE_ENTERPRISE_USER_CERTIFICATE,
};

// A key name for the Enterprise Machine Key.  This key should always be stored
// as a DEVICE_KEY.
CHROMEOS_EXPORT extern const char kEnterpriseMachineKey[];

// A key name for the Enterprise User Key.  This key should always be stored as
// a USER_KEY.
CHROMEOS_EXPORT extern const char kEnterpriseUserKey[];

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROMEOS_ATTESTATION_ATTESTATION_CONSTANTS_H_
