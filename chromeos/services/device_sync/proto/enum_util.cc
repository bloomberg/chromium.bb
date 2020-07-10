// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/proto/enum_util.h"

namespace chromeos {

namespace device_sync {

std::ostream& operator<<(std::ostream& stream,
                         const cryptauth::DeviceType& device_type) {
  switch (device_type) {
    case cryptauth::DeviceType::ANDROID:
      stream << "[Android]";
      break;
    case cryptauth::DeviceType::CHROME:
      stream << "[Chrome]";
      break;
    case cryptauth::DeviceType::IOS:
      stream << "[iOS]";
      break;
    case cryptauth::DeviceType::BROWSER:
      stream << "[Browser]";
      break;
    default:
      stream << "[Unknown device type]";
      break;
  }
  return stream;
}

cryptauth::DeviceType DeviceTypeStringToEnum(
    const std::string& device_type_as_string) {
  if (device_type_as_string == "android")
    return cryptauth::DeviceType::ANDROID;
  if (device_type_as_string == "chrome")
    return cryptauth::DeviceType::CHROME;
  if (device_type_as_string == "ios")
    return cryptauth::DeviceType::IOS;
  if (device_type_as_string == "browser")
    return cryptauth::DeviceType::BROWSER;
  return cryptauth::DeviceType::UNKNOWN;
}

std::string DeviceTypeEnumToString(cryptauth::DeviceType device_type) {
  switch (device_type) {
    case cryptauth::DeviceType::ANDROID:
      return "android";
    case cryptauth::DeviceType::CHROME:
      return "chrome";
    case cryptauth::DeviceType::IOS:
      return "ios";
    case cryptauth::DeviceType::BROWSER:
      return "browser";
    default:
      return "unknown";
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const cryptauth::SoftwareFeature& software_feature) {
  switch (software_feature) {
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST:
      stream << "[Better Together host]";
      break;
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      stream << "[Better Together client]";
      break;
    case cryptauth::SoftwareFeature::EASY_UNLOCK_HOST:
      stream << "[EasyUnlock host]";
      break;
    case cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT:
      stream << "[EasyUnlock client]";
      break;
    case cryptauth::SoftwareFeature::MAGIC_TETHER_HOST:
      stream << "[Instant Tethering host]";
      break;
    case cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT:
      stream << "[Instant Tethering client]";
      break;
    case cryptauth::SoftwareFeature::SMS_CONNECT_HOST:
      stream << "[SMS Connect host]";
      break;
    case cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT:
      stream << "[SMS Connect client]";
      break;
    default:
      stream << "[unknown software feature]";
      break;
  }
  return stream;
}

cryptauth::SoftwareFeature SoftwareFeatureStringToEnum(
    const std::string& software_feature_as_string) {
  if (software_feature_as_string == "betterTogetherHost")
    return cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST;
  if (software_feature_as_string == "betterTogetherClient")
    return cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT;
  if (software_feature_as_string == "easyUnlockHost")
    return cryptauth::SoftwareFeature::EASY_UNLOCK_HOST;
  if (software_feature_as_string == "easyUnlockClient")
    return cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT;
  if (software_feature_as_string == "magicTetherHost")
    return cryptauth::SoftwareFeature::MAGIC_TETHER_HOST;
  if (software_feature_as_string == "magicTetherClient")
    return cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT;
  if (software_feature_as_string == "smsConnectHost")
    return cryptauth::SoftwareFeature::SMS_CONNECT_HOST;
  if (software_feature_as_string == "smsConnectClient")
    return cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT;

  return cryptauth::SoftwareFeature::UNKNOWN_FEATURE;
}

std::string SoftwareFeatureEnumToString(
    cryptauth::SoftwareFeature software_feature) {
  switch (software_feature) {
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST:
      return "betterTogetherHost";
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      return "betterTogetherClient";
    case cryptauth::SoftwareFeature::EASY_UNLOCK_HOST:
      return "easyUnlockHost";
    case cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT:
      return "easyUnlockClient";
    case cryptauth::SoftwareFeature::MAGIC_TETHER_HOST:
      return "magicTetherHost";
    case cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT:
      return "magicTetherClient";
    case cryptauth::SoftwareFeature::SMS_CONNECT_HOST:
      return "smsConnectHost";
    case cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT:
      return "smsConnectClient";
    default:
      return "unknownFeature";
  }
}

std::string SoftwareFeatureEnumToStringAllCaps(
    cryptauth::SoftwareFeature software_feature) {
  switch (software_feature) {
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST:
      return "BETTER_TOGETHER_HOST";
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      return "BETTER_TOGETHER_CLIENT";
    case cryptauth::SoftwareFeature::EASY_UNLOCK_HOST:
      return "EASY_UNLOCK_HOST";
    case cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT:
      return "EASY_UNLOCK_CLIENT";
    case cryptauth::SoftwareFeature::MAGIC_TETHER_HOST:
      return "MAGIC_TETHER_HOST";
    case cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT:
      return "MAGIC_TETHER_CLIENT";
    case cryptauth::SoftwareFeature::SMS_CONNECT_HOST:
      return "SMS_CONNECT_HOST";
    case cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT:
      return "SMS_CONNECT_CLIENT";
    default:
      return "UNKNOWN_FEATURE";
  }
}

}  // namespace device_sync

}  // namespace chromeos
