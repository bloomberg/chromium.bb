// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_TYPE_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_TYPE_H_

#include <ostream>
#include <string>

#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/software_feature.h"

namespace chromeos {

namespace device_sync {

// The BetterTogether feature types used by the CryptAuth v2 backend. Each
// feature has a separate type for the "supported" state and "enabled" state of
// the feature. Currently, the "supported" types are only used for the CryptAuth
// v2 DeviceSync BatchGetFeatureStatuses RPC. Each enum value is uniquely mapped
// to a string used in the protos and understood by CryptAuth.
//
// Example: The following FeatureStatus messages received in a
// BatchGetFeatureStatuses response would indicate that BetterTogether host is
// supported but not enabled:
//   [
//     message FeatureStatus {
//       string feature_type = "BETTER_TOGETHER_HOST_SUPPORTED";
//       bool enabled = true;
//     },
//     message FeatureStatus {
//       string feature_type = "BETTER_TOGETHER_HOST";
//       bool enabled = false;
//     }
//   ]
enum class CryptAuthFeatureType {
  // Support for multi-device features in general.
  kBetterTogetherHostSupported,
  kBetterTogetherHostEnabled,
  kBetterTogetherClientSupported,
  kBetterTogetherClientEnabled,

  // Smart Lock, which gives the user the ability to unlock and/or sign into a
  // Chromebook using an Android phone.
  kEasyUnlockHostSupported,
  kEasyUnlockHostEnabled,
  kEasyUnlockClientSupported,
  kEasyUnlockClientEnabled,

  // Instant Tethering, which gives the user the ability to use an Android
  // phone's Internet connection on a Chromebook.
  kMagicTetherHostSupported,
  kMagicTetherHostEnabled,
  kMagicTetherClientSupported,
  kMagicTetherClientEnabled,

  // Messages for Web, which gives the user the ability to sync messages (e.g.,
  // SMS) between an Android phone and a Chromebook.
  kSmsConnectHostSupported,
  kSmsConnectHostEnabled,
  kSmsConnectClientSupported,
  kSmsConnectClientEnabled,
};

const base::flat_set<CryptAuthFeatureType>& GetAllCryptAuthFeatureTypes();
const base::flat_set<CryptAuthFeatureType>& GetSupportedCryptAuthFeatureTypes();
const base::flat_set<CryptAuthFeatureType>& GetEnabledCryptAuthFeatureTypes();
const base::flat_set<std::string>& GetAllCryptAuthFeatureTypeStrings();

// Provides a unique mapping between each CryptAuthFeatureType enum value and
// the corresponding string used in the protos and understood by CryptAuth.
// CryptAuthFeatureTypeFromString returns null if |feature_type_string| does not
// map to a known CryptAuthFeatureType.
const char* CryptAuthFeatureTypeToString(CryptAuthFeatureType feature_type);
base::Optional<CryptAuthFeatureType> CryptAuthFeatureTypeFromString(
    const std::string& feature_type_string);

// Provides a mapping between CryptAuthFeatureTypes and SoftwareFeatures.
//
// The "enabled" and "supported" feature types are mapped to the same
// SoftwareFeature. For example,
// CryptAuthFeatureType::kBetterTogetherHostEnabled and
// CryptAuthFeatureType::kBetterTogetherHostSupported are both mapped to
// SoftwareFeature::kBetterTogetherHost.
//
// A SoftwareFeature is mapped to the "enabled" CryptAuthFeatureType. For
// example, SoftwareFeature::kBetterTogetherHost maps to
// CryptAuthFeatureType::kBetterTogetherHostEnabled.
multidevice::SoftwareFeature CryptAuthFeatureTypeToSoftwareFeature(
    CryptAuthFeatureType feature_type);
CryptAuthFeatureType CryptAuthFeatureTypeFromSoftwareFeature(
    multidevice::SoftwareFeature software_feature);

std::ostream& operator<<(std::ostream& stream,
                         CryptAuthFeatureType feature_type);

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_TYPE_H_
