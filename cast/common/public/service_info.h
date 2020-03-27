// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_PUBLIC_SERVICE_INFO_H_
#define CAST_COMMON_PUBLIC_SERVICE_INFO_H_

#include <memory>
#include <string>

#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace cast {

// Constants to identify a CastV2 instance with DNS-SD.
static constexpr char kCastV2ServiceId[] = "_googlecast._tcp";
static constexpr char kCastV2DomainId[] = "local";

// Constants to be used as keys when storing data inside of a DNS-SD TXT record.
static constexpr char kUniqueIdKey[] = "id";
static constexpr char kVersionId[] = "ve";
static constexpr char kCapabilitiesId[] = "ca";
static constexpr char kStatusId[] = "st";
static constexpr char kFriendlyNameId[] = "fn";
static constexpr char kModelNameId[] = "mn";

// This represents the ‘st’ flag in the CastV2 TXT record.
enum ReceiverStatus {
  // The receiver is idle and does not need to be connected now.
  kIdle = 0,

  // The receiver is hosting an activity and invites the sender to join.  The
  // receiver should connect to the running activity using the channel
  // establishment protocol, and then query the activity to determine the next
  // step, such as showing a description of the activity and prompting the user
  // to launch the corresponding app.
  kBusy = 1,
  kJoin = kBusy
};

// This represents the ‘ca’ field in the CastV2 spec.
enum ReceiverCapabilities : uint64_t {
  kNone = 0x00,
  kHasVideoOutput = 0x01 << 0,
  kHasVideoInput = 0x01 << 1,
  kHasAudioOutput = 0x01 << 2,
  kHasAudioInput = 0x01 << 3,
  kIsDevModeEnabled = 0x01 << 4,
};

static constexpr uint8_t kCurrentCastVersion = 2;
static constexpr ReceiverCapabilities kDefaultCapabilities =
    ReceiverCapabilities::kNone;

// This is the top-level service info class for CastV2. It describes a specific
// service instance.
// TODO(crbug.com/openscreen/112): Rename this to CastReceiverInfo or similar.
struct ServiceInfo {
  // returns the instance id associated with this ServiceInfo instance.
  const std::string& GetInstanceId() const;

  // Returns whether all fields of this ServiceInfo are valid.
  bool IsValid() const;

  // Addresses for the service. Present if an address of this address type
  // exists and empty otherwise.
  IPAddress v4_address;
  IPAddress v6_address;

  // Port at which this service can be reached.
  uint16_t port;

  // A UUID for the Cast receiver. This should be a universally unique
  // identifier for the receiver, and should (but does not have to be) be stable
  // across factory resets.
  std::string unique_id;

  // Cast protocol version supported. Begins at 2 and is incremented by 1 with
  // each version.
  uint8_t protocol_version = kCurrentCastVersion;

  // Capabilities supported by this service instance.
  ReceiverCapabilities capabilities = kDefaultCapabilities;

  // Status of the service instance.
  ReceiverStatus status = ReceiverStatus::kIdle;

  // The model name of the device, e.g. “Eureka v1”, “Mollie”.
  std::string model_name;

  // The friendly name of the device, e.g. “Living Room TV".
  std::string friendly_name;

 private:
  mutable std::string instance_id_ = "";
};

inline bool operator==(const ServiceInfo& lhs, const ServiceInfo& rhs) {
  return lhs.v4_address == rhs.v4_address && lhs.v6_address == rhs.v6_address &&
         lhs.port == rhs.port && lhs.unique_id == rhs.unique_id &&
         lhs.protocol_version == rhs.protocol_version &&
         lhs.capabilities == rhs.capabilities && lhs.status == rhs.status &&
         lhs.model_name == rhs.model_name &&
         lhs.friendly_name == rhs.friendly_name;
}

inline bool operator!=(const ServiceInfo& lhs, const ServiceInfo& rhs) {
  return !(lhs == rhs);
}

// Functions responsible for converting between CastV2 and DNS-SD
// representations of a service instance.
discovery::DnsSdInstanceRecord ServiceInfoToDnsSdRecord(
    const ServiceInfo& service);

ErrorOr<ServiceInfo> DnsSdRecordToServiceInfo(
    const discovery::DnsSdInstanceRecord& service);

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_PUBLIC_SERVICE_INFO_H_
