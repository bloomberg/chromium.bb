// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/connection_priority.h"

#include "base/logging.h"

namespace chromeos {

namespace tether {

ConnectionPriority PriorityForMessageType(MessageType message_type) {
  switch (message_type) {
    case CONNECT_TETHERING_REQUEST:
    case DISCONNECT_TETHERING_REQUEST:
      return ConnectionPriority::CONNECTION_PRIORITY_HIGH;
    case KEEP_ALIVE_TICKLE:
      return ConnectionPriority::CONNECTION_PRIORITY_MEDIUM;
    default:
      return ConnectionPriority::CONNECTION_PRIORITY_LOW;
  }
}

ConnectionPriority HighestPriorityForMessageTypes(
    std::set<MessageType> message_types) {
  DCHECK(!message_types.empty());

  ConnectionPriority highest_priority =
      ConnectionPriority::CONNECTION_PRIORITY_LOW;
  for (const auto& message_type : message_types) {
    ConnectionPriority priority_for_type = PriorityForMessageType(message_type);
    if (priority_for_type > highest_priority)
      highest_priority = priority_for_type;
  }

  return highest_priority;
}

}  // namespace tether

}  // namespace chromeos
