// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/public/service_info.h"

#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_replace.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace {

// The mask for the set of supported capabilities in v2 of the Cast protocol.
const uint16_t kCapabilitiesMask = 0x1F;

// Maximum size for registered MDNS service instance names.
const size_t kMaxDeviceNameSize = 63;

// Maximum size for the device model prefix at start of MDNS service instance
// names. Any model names that are larger than this size will be truncated.
const size_t kMaxDeviceModelSize = 20;

// Build the MDNS instance name for service. This will be the device model (up
// to 20 bytes) appended with the virtual device ID (device UUID) and optionally
// appended with extension at the end to resolve name conflicts. The total MDNS
// service instance name is kept below 64 bytes so it can easily fit into a
// single domain name label.
//
// NOTE: This value is based on what is currently done by Eureka, not what is
// called out in the CastV2 spec. Eureka uses |model|-|uuid|, so the same
// convention will be followed here. That being said, the Eureka receiver does
// not use the instance ID in any way, so the specific calculation used should
// not be important.
std::string CalculateInstanceId(const ServiceInfo& info) {
  // First set the device model, truncated to 20 bytes at most. Replace any
  // whitespace characters (" ") with hyphens ("-") in the device model before
  // truncation.
  std::string instance_name =
      absl::StrReplaceAll(info.model_name, {{" ", "-"}});
  instance_name = std::string(instance_name, 0, kMaxDeviceModelSize);

  // Append the virtual device ID to the instance name separated by a single
  // '-' character if not empty. Strip all hyphens from the device ID prior
  // to appending it.
  std::string device_id = absl::StrReplaceAll(info.unique_id, {{"-", ""}});

  if (!instance_name.empty()) {
    instance_name.push_back('-');
  }
  instance_name.append(device_id);

  return std::string(instance_name, 0, kMaxDeviceNameSize);
}

// NOTE: Eureka uses base::StringToUint64 which takes in a string and reads it
// left to right, converts it to a number sequence, and uses the sequence to
// calculate the resulting integer. This process assumes that the input is in
// base 10. For example, ['1', '2', '3'] converts to 123.
//
// The below 2 functions re-create this logic for converting to and from this
// encoding scheme.
inline std::string EncodeIntegerString(uint64_t value) {
  return std::to_string(value);
}

ErrorOr<uint64_t> DecodeIntegerString(const std::string& value) {
  uint64_t result;
  if (!absl::SimpleAtoi(value, &result)) {
    return Error::Code::kParameterInvalid;
  }

  return result;
}

// Attempts to parse the string present at the provided |key| in the TXT record
// |txt|, placing the result into |result| on success and error into |error| on
// failure.
bool TryParseString(const discovery::DnsSdTxtRecord& txt,
                    const std::string& key,
                    Error* error,
                    std::string* result) {
  const ErrorOr<discovery::DnsSdTxtRecord::ValueRef> value = txt.GetValue(key);
  if (value.is_error()) {
    *error = value.error();
    return false;
  }

  const std::vector<uint8_t>& txt_value = value.value().get();
  *result = std::string(txt_value.begin(), txt_value.end());
  return true;
}
// Attempts to parse the uint8_t present at the provided |key| in the TXT record
// |txt|, placing the result into |result| on success and error into |error| on
// failure.
bool TryParseInt(const discovery::DnsSdTxtRecord& txt,
                 const std::string& key,
                 Error* error,
                 uint8_t* result) {
  const ErrorOr<discovery::DnsSdTxtRecord::ValueRef> value = txt.GetValue(key);
  if (value.is_error()) {
    *error = value.error();
    return false;
  }

  const std::vector<uint8_t>& txt_value = value.value().get();
  if (txt_value.size() != 1) {
    *error = Error::Code::kParameterInvalid;
    return false;
  }

  *result = txt_value[0];
  return true;
}

// Simplifies logic below by changing error into an output parameter instead of
// a return value.
bool IsError(Error error, Error* result) {
  if (error.ok()) {
    return false;
  } else {
    *result = error;
    return true;
  }
}

}  // namespace

const std::string& ServiceInfo::GetInstanceId() const {
  if (instance_id_ == std::string("")) {
    instance_id_ = CalculateInstanceId(*this);
  }

  return instance_id_;
}

bool ServiceInfo::IsValid() const {
  std::string instance_id = GetInstanceId();
  if (!discovery::IsInstanceValid(instance_id)) {
    return false;
  }

  const std::string capabilities_str = EncodeIntegerString(capabilities);
  if (!discovery::DnsSdTxtRecord::IsValidTxtValue(kUniqueIdKey, unique_id) ||
      !discovery::DnsSdTxtRecord::IsValidTxtValue(kVersionId,
                                                  protocol_version) ||
      !discovery::DnsSdTxtRecord::IsValidTxtValue(kCapabilitiesId,
                                                  capabilities_str) ||
      !discovery::DnsSdTxtRecord::IsValidTxtValue(kStatusId, status) ||
      !discovery::DnsSdTxtRecord::IsValidTxtValue(kFriendlyNameId,
                                                  friendly_name) ||
      !discovery::DnsSdTxtRecord::IsValidTxtValue(kModelNameId, model_name)) {
    return false;
  }

  return port && (v4_address || v6_address);
}

discovery::DnsSdInstanceRecord ServiceInfoToDnsSdRecord(
    const ServiceInfo& service) {
  OSP_DCHECK(discovery::IsServiceValid(kCastV2ServiceId));
  OSP_DCHECK(discovery::IsDomainValid(kCastV2DomainId));

  std::string instance_id = service.GetInstanceId();
  OSP_DCHECK(discovery::IsInstanceValid(instance_id));

  const std::string capabilities_str =
      EncodeIntegerString(service.capabilities);

  discovery::DnsSdTxtRecord txt;
  Error error;
  const bool set_txt =
      !IsError(txt.SetValue(kUniqueIdKey, service.unique_id), &error) &&
      !IsError(txt.SetValue(kVersionId, service.protocol_version), &error) &&
      !IsError(txt.SetValue(kCapabilitiesId, capabilities_str), &error) &&
      !IsError(txt.SetValue(kStatusId, service.status), &error) &&
      !IsError(txt.SetValue(kFriendlyNameId, service.friendly_name), &error) &&
      !IsError(txt.SetValue(kModelNameId, service.model_name), &error);
  OSP_DCHECK(set_txt);

  OSP_DCHECK(service.port);
  OSP_DCHECK(service.v4_address || service.v6_address);
  if (service.v4_address && service.v6_address) {
    return discovery::DnsSdInstanceRecord(
        instance_id, kCastV2ServiceId, kCastV2DomainId,
        {service.v4_address, service.port}, {service.v6_address, service.port},
        std::move(txt));
  } else {
    const IPAddress& address =
        service.v4_address ? service.v4_address : service.v6_address;
    return discovery::DnsSdInstanceRecord(
        instance_id, kCastV2ServiceId, kCastV2DomainId, {address, service.port},
        std::move(txt));
  }
}

ErrorOr<ServiceInfo> DnsSdRecordToServiceInfo(
    const discovery::DnsSdInstanceRecord& instance) {
  if (instance.service_id() != kCastV2ServiceId) {
    return Error::Code::kParameterInvalid;
  }

  ServiceInfo record;
  record.v4_address = instance.address_v4().address;
  record.v6_address = instance.address_v6().address;
  record.port = instance.address_v4().port ? instance.address_v4().port
                                           : instance.address_v6().port;

  const auto& txt = instance.txt();
  std::string capabilities_base64;
  std::string unique_id;
  uint8_t status;
  Error error;
  if (!TryParseInt(txt, kVersionId, &error, &record.protocol_version) ||
      !TryParseInt(txt, kStatusId, &error, &status) ||
      !TryParseString(txt, kFriendlyNameId, &error, &record.friendly_name) ||
      !TryParseString(txt, kModelNameId, &error, &record.model_name) ||
      !TryParseString(txt, kCapabilitiesId, &error, &capabilities_base64) ||
      !TryParseString(txt, kUniqueIdKey, &error, &record.unique_id)) {
    return error;
  }

  record.status = static_cast<ReceiverStatus>(status);

  const ErrorOr<uint64_t> capabilities_flags =
      DecodeIntegerString(capabilities_base64);
  if (capabilities_flags.is_error()) {
    return capabilities_flags.error();
  }
  record.capabilities = static_cast<ReceiverCapabilities>(
      capabilities_flags.value() & kCapabilitiesMask);

  return record;
}

}  // namespace cast
}  // namespace openscreen
