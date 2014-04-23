// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_IDS_H_
#define MOJO_SERVICES_VIEW_MANAGER_IDS_H_

#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace services {
namespace view_manager {

// Adds a bit of type safety to node ids.
struct MOJO_VIEW_MANAGER_EXPORT NodeId {
  NodeId(uint16_t connection_id, uint16_t node_id)
      : connection_id(connection_id),
        node_id(node_id) {}
  NodeId() : connection_id(0), node_id(0) {}

  bool operator==(const NodeId& other) const {
    return other.connection_id == connection_id &&
        other.node_id == node_id;
  }

  uint16_t connection_id;
  uint16_t node_id;
};

// Adds a bit of type safety to view ids.
struct MOJO_VIEW_MANAGER_EXPORT ViewId {
  ViewId(uint16_t connection_id, uint16_t view_id)
      : connection_id(connection_id),
        view_id(view_id) {}
  ViewId() : connection_id(0), view_id(0) {}

  bool operator==(const ViewId& other) const {
    return other.connection_id == connection_id &&
        other.view_id == view_id;
  }

  uint16_t connection_id;
  uint16_t view_id;
};

// Functions for converting to/from structs and transport values.
inline uint16_t FirstIdFromTransportId(uint32_t id) {
  return static_cast<uint16_t>((id >> 16) & 0xFFFF);
}

inline uint16_t SecondIdFromTransportId(uint32_t id) {
  return static_cast<uint16_t>(id & 0xFFFF);
}

inline NodeId NodeIdFromTransportId(uint32_t id) {
  return NodeId(FirstIdFromTransportId(id), SecondIdFromTransportId(id));
}

inline uint32_t NodeIdToTransportId(const NodeId& id) {
  return (id.connection_id << 16) | id.node_id;
}

inline ViewId ViewIdFromTransportId(uint32_t id) {
  return ViewId(FirstIdFromTransportId(id), SecondIdFromTransportId(id));
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_IDS_H_
