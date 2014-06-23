// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_event_notification_details.h"

namespace content {

AXEventNotificationDetails::AXEventNotificationDetails(
    int node_id_to_clear,
    const std::vector<ui::AXNodeData>& nodes,
    ui::AXEvent event_type,
    int id,
    int process_id,
    int routing_id)
    : node_id_to_clear(node_id_to_clear),
      nodes(nodes),
      event_type(event_type),
      id(id),
      process_id(process_id),
      routing_id(routing_id) {
}

AXEventNotificationDetails::~AXEventNotificationDetails() {}

}  // namespace content
