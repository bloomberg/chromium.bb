// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_feature_type.h"

namespace chromeos {

namespace device_sync {

std::string CryptAuthFeatureTypeToString(
    const CryptAuthFeatureType& feature_type) {
  switch (feature_type) {
    case CryptAuthFeatureType::kBetterTogetherHostSupported:
      return "BetterTogetherHostSupported";
    case CryptAuthFeatureType::kBetterTogetherHostEnabled:
      return "BetterTogetherHostEnabled";
    case CryptAuthFeatureType::kBetterTogetherClientSupported:
      return "BetterTogetherClientSupported";
    case CryptAuthFeatureType::kBetterTogetherClientEnabled:
      return "BetterTogetherClientEnabled";
    case CryptAuthFeatureType::kEasyUnlockHostSupported:
      return "EasyUnlockHostSupported";
    case CryptAuthFeatureType::kEasyUnlockHostEnabled:
      return "EasyUnlockHostEnabled";
    case CryptAuthFeatureType::kEasyUnlockClientSupported:
      return "EasyUnlockClientSupported";
    case CryptAuthFeatureType::kEasyUnlockClientEnabled:
      return "EasyUnlockClientEnabled";
    case CryptAuthFeatureType::kMagicTetherHostSupported:
      return "MagicTetherHostSupported";
    case CryptAuthFeatureType::kMagicTetherHostEnabled:
      return "MagicTetherHostEnabled";
    case CryptAuthFeatureType::kMagicTetherClientSupported:
      return "MagicTetherClientSupported";
    case CryptAuthFeatureType::kMagicTetherClientEnabled:
      return "MagicTetherClientEnabled";
    case CryptAuthFeatureType::kSmsConnectHostSupported:
      return "SmsConnectHostSupported";
    case CryptAuthFeatureType::kSmsConnectHostEnabled:
      return "SmsConnectHostEnabled";
    case CryptAuthFeatureType::kSmsConnectClientSupported:
      return "SmsConnectClientSupported";
    case CryptAuthFeatureType::kSmsConnectClientEnabled:
      return "SmsConnectClientEnabled";
  }
}

base::Optional<CryptAuthFeatureType> CryptAuthFeatureTypeFromString(
    const std::string& feature_type_string) {
  if (feature_type_string == "BetterTogetherHostSupported")
    return CryptAuthFeatureType::kBetterTogetherHostSupported;
  if (feature_type_string == "BetterTogetherHostEnabled")
    return CryptAuthFeatureType::kBetterTogetherHostEnabled;
  if (feature_type_string == "BetterTogetherClientSupported")
    return CryptAuthFeatureType::kBetterTogetherClientSupported;
  if (feature_type_string == "BetterTogetherClientEnabled")
    return CryptAuthFeatureType::kBetterTogetherClientEnabled;
  if (feature_type_string == "EasyUnlockHostSupported")
    return CryptAuthFeatureType::kEasyUnlockHostSupported;
  if (feature_type_string == "EasyUnlockHostEnabled")
    return CryptAuthFeatureType::kEasyUnlockHostEnabled;
  if (feature_type_string == "EasyUnlockClientSupported")
    return CryptAuthFeatureType::kEasyUnlockClientSupported;
  if (feature_type_string == "EasyUnlockClientEnabled")
    return CryptAuthFeatureType::kEasyUnlockClientEnabled;
  if (feature_type_string == "MagicTetherHostSupported")
    return CryptAuthFeatureType::kMagicTetherHostSupported;
  if (feature_type_string == "MagicTetherHostEnabled")
    return CryptAuthFeatureType::kMagicTetherHostEnabled;
  if (feature_type_string == "MagicTetherClientSupported")
    return CryptAuthFeatureType::kMagicTetherClientSupported;
  if (feature_type_string == "MagicTetherClientEnabled")
    return CryptAuthFeatureType::kMagicTetherClientEnabled;
  if (feature_type_string == "SmsConnectHostSupported")
    return CryptAuthFeatureType::kSmsConnectHostSupported;
  if (feature_type_string == "SmsConnectHostEnabled")
    return CryptAuthFeatureType::kSmsConnectHostEnabled;
  if (feature_type_string == "SmsConnectClientSupported")
    return CryptAuthFeatureType::kSmsConnectClientSupported;
  if (feature_type_string == "SmsConnectClientEnabled")
    return CryptAuthFeatureType::kSmsConnectClientEnabled;

  return base::nullopt;
}

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthFeatureType& feature_type) {
  stream << CryptAuthFeatureTypeToString(feature_type);
  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
