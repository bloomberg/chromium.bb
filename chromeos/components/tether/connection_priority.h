// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_CONNECTION_PRIORITY_H_
#define CHROMEOS_COMPONENTS_TETHER_CONNECTION_PRIORITY_H_

#include <set>

#include "chromeos/components/tether/proto/tether.pb.h"

namespace chromeos {

namespace tether {

// Priority of connections; lower-priority connections will remain queued until
// higher-priority connections are complete.
enum class ConnectionPriority {
  // Low-priority applies to background connections which were not triggered by
  // the user and do not have a strict latency requirement.
  //   Example: TetherAvailabilityRequest messages.
  CONNECTION_PRIORITY_LOW = 1,

  // Medium-priority applies to connections which should finish within a
  // reasonable amount of time but whose latency is not directly observed by
  // the user.
  //   Example: KeepAliveTickle messages.
  CONNECTION_PRIORITY_MEDIUM = 2,

  // High-priority applies to connections whose latency is directly observable
  // by the user, usually as part of the UI.
  //   Example: ConnectTetheringRequest and DisconnectTetheringRequest messages.
  CONNECTION_PRIORITY_HIGH = 3
};

ConnectionPriority PriorityForMessageType(MessageType message_type);
ConnectionPriority HighestPriorityForMessageTypes(
    std::set<MessageType> message_types);

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_CONNECTION_PRIORITY_H_
