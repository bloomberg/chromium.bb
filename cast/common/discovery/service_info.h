// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_DISCOVERY_SERVICE_INFO_H_
#define CAST_COMMON_DISCOVERY_SERVICE_INFO_H_

#include <memory>
#include <string>

#include "platform/base/ip_address.h"

namespace openscreen {
namespace cast {

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
  kHasVideoOutput = 0x01 << 0,
  kHasVideoInput = 0x01 << 1,
  kHasAudioOutput = 0x01 << 2,
  kHasAudioInput = 0x01 << 3,
  kIsDevModeEnabled = 0x01 << 4,
};

// This is the top-level service info class for CastV2. It describes a specific
// service instance.
struct ServiceInfo {
  // Endpoints for the service. Present if an endpoint of this address type
  // exists and empty otherwise.
  IPEndpoint v4_address;
  IPEndpoint v6_address;

  // A UUID for the Cast receiver. This should be a universally unique
  // identifier for the receiver, and should (but does not have to be) be stable
  // across factory resets.
  std::string unique_id;

  // Cast protocol version supported. Begins at 2 and is incremented by 1 with
  // each version.
  uint8_t protocol_version;

  // Capabilities supported by this service instance.
  ReceiverCapabilities capabilities;

  // Status of the service instance.
  ReceiverStatus status;

  // The model name of the device, e.g. “Eureka v1”, “Mollie”.
  std::string model_name;

  // The friendly name of the device, e.g. “Living Room TV".
  std::string friendly_name;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_DISCOVERY_SERVICE_INFO_H_
