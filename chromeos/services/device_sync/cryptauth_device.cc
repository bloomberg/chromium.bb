// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device.h"

#include "base/strings/string_number_conversions.h"
#include "base/util/values/values_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/value_string_encoding.h"

namespace chromeos {

namespace device_sync {

namespace {

// Strings used as the DictionaryValue keys in {From,As}DictionaryValue().
const char kInstanceIdDictKey[] = "instance_id";
const char kDeviceNameDictKey[] = "device_name";
const char kLastUpdateTimeDictKey[] = "last_update_time";
const char kDeviceBetterTogetherPublicKeyDictKey[] =
    "device_better_together_public_key";
const char kBetterTogetherDeviceMetadataDictKey[] =
    "better_together_device_metadata";
const char kFeatureStatesDictKey[] = "feature_states";

base::Optional<
    std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>>
FeatureStatesFromDictionary(const base::Value* dict) {
  if (!dict || !dict->is_dict())
    return base::nullopt;

  std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>
      feature_states;
  for (const auto& feature_state_pair : dict->DictItems()) {
    int feature;
    if (!base::StringToInt(feature_state_pair.first, &feature) ||
        !feature_state_pair.second.is_int()) {
      return base::nullopt;
    }

    feature_states[static_cast<multidevice::SoftwareFeature>(feature)] =
        static_cast<multidevice::SoftwareFeatureState>(
            feature_state_pair.second.GetInt());
  }

  return feature_states;
}

base::Value FeatureStatesToDictionary(
    const std::map<multidevice::SoftwareFeature,
                   multidevice::SoftwareFeatureState>& feature_states) {
  base::Value dict(base::Value::Type::DICTIONARY);
  for (const auto& feature_state_pair : feature_states) {
    dict.SetIntKey(
        base::NumberToString(static_cast<int>(feature_state_pair.first)),
        static_cast<int>(feature_state_pair.second));
  }

  return dict;
}

}  // namespace

// static
base::Optional<CryptAuthDevice> CryptAuthDevice::FromDictionary(
    const base::Value& dict) {
  if (!dict.is_dict())
    return base::nullopt;

  base::Optional<std::string> instance_id =
      util::DecodeFromValueString(dict.FindKey(kInstanceIdDictKey));
  if (!instance_id || instance_id->empty())
    return base::nullopt;

  base::Optional<std::string> device_name =
      util::DecodeFromValueString(dict.FindKey(kDeviceNameDictKey));
  if (!device_name || device_name->empty())
    return base::nullopt;

  base::Optional<std::string> device_better_together_public_key =
      util::DecodeFromValueString(
          dict.FindKey(kDeviceBetterTogetherPublicKeyDictKey));
  if (!device_better_together_public_key ||
      device_better_together_public_key->empty()) {
    return base::nullopt;
  }

  base::Optional<base::Time> last_update_time =
      ::util::ValueToTime(dict.FindKey(kLastUpdateTimeDictKey));
  if (!last_update_time)
    return base::nullopt;

  base::Optional<cryptauthv2::BetterTogetherDeviceMetadata>
      better_together_device_metadata = util::DecodeProtoMessageFromValueString<
          cryptauthv2::BetterTogetherDeviceMetadata>(
          dict.FindKey(kBetterTogetherDeviceMetadataDictKey));
  if (!better_together_device_metadata)
    return base::nullopt;

  base::Optional<
      std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>>
      feature_states =
          FeatureStatesFromDictionary(dict.FindDictKey(kFeatureStatesDictKey));
  if (!feature_states)
    return base::nullopt;

  return CryptAuthDevice(*instance_id, *device_name,
                         *device_better_together_public_key, *last_update_time,
                         *better_together_device_metadata, *feature_states);
}

CryptAuthDevice::CryptAuthDevice(
    const std::string& instance_id,
    const std::string& device_name,
    const std::string& device_better_together_public_key,
    const base::Time& last_update_time,
    const cryptauthv2::BetterTogetherDeviceMetadata&
        better_together_device_metadata,
    const std::map<multidevice::SoftwareFeature,
                   multidevice::SoftwareFeatureState>& feature_states)
    : instance_id_(instance_id),
      device_name_(device_name),
      device_better_together_public_key_(device_better_together_public_key),
      last_update_time_(last_update_time),
      better_together_device_metadata_(better_together_device_metadata),
      feature_states_(feature_states) {
  DCHECK(!instance_id.empty());
  DCHECK(!device_name.empty());
  DCHECK(!device_better_together_public_key.empty());
  DCHECK(!last_update_time.is_null());
}

CryptAuthDevice::CryptAuthDevice(const CryptAuthDevice&) = default;

CryptAuthDevice::~CryptAuthDevice() = default;

base::Value CryptAuthDevice::AsDictionary() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kInstanceIdDictKey, util::EncodeAsValueString(instance_id_));
  dict.SetKey(kDeviceNameDictKey, util::EncodeAsValueString(device_name_));
  dict.SetKey(kDeviceBetterTogetherPublicKeyDictKey,
              util::EncodeAsValueString(device_better_together_public_key_));
  dict.SetKey(kLastUpdateTimeDictKey, ::util::TimeToValue(last_update_time_));
  dict.SetKey(
      kBetterTogetherDeviceMetadataDictKey,
      util::EncodeProtoMessageAsValueString(&better_together_device_metadata_));
  dict.SetKey(kFeatureStatesDictKey,
              FeatureStatesToDictionary(feature_states_));

  return dict;
}

bool CryptAuthDevice::operator==(const CryptAuthDevice& other) const {
  return instance_id_ == other.instance_id_ &&
         device_name_ == other.device_name_ &&
         device_better_together_public_key_ ==
             other.device_better_together_public_key_ &&
         last_update_time_ == other.last_update_time_ &&
         better_together_device_metadata_.SerializeAsString() ==
             other.better_together_device_metadata_.SerializeAsString() &&
         feature_states_ == other.feature_states_;
}

bool CryptAuthDevice::operator!=(const CryptAuthDevice& other) const {
  return !(*this == other);
}

}  // namespace device_sync

}  // namespace chromeos
