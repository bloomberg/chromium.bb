// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_IDS_H_
#define COMPONENTS_MUS_WS_IDS_H_

#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/util.h"

namespace mus {

namespace ws {

// Connection id is used to indicate no connection. That is, no WindowTreeImpl
// ever gets this id.
const ConnectionSpecificId kInvalidConnectionId = 0;

// Adds a bit of type safety to window ids.
struct WindowId {
  WindowId(ConnectionSpecificId connection_id, ConnectionSpecificId window_id)
      : connection_id(connection_id), window_id(window_id) {}
  WindowId() : connection_id(0), window_id(0) {}

  bool operator==(const WindowId& other) const {
    return other.connection_id == connection_id && other.window_id == window_id;
  }

  bool operator!=(const WindowId& other) const { return !(*this == other); }

  bool operator<(const WindowId& other) const {
    if (connection_id == other.connection_id)
      return window_id < other.window_id;

    return connection_id < other.connection_id;
  }

  ConnectionSpecificId connection_id;
  ConnectionSpecificId window_id;
};

inline WindowId WindowIdFromTransportId(Id id) {
  return WindowId(HiWord(id), LoWord(id));
}

inline Id WindowIdToTransportId(const WindowId& id) {
  return (id.connection_id << 16) | id.window_id;
}

// Returns a WindowId that is reserved to indicate no window. That is, no window
// will
// ever be created with this id.
inline WindowId InvalidWindowId() {
  return WindowId(kInvalidConnectionId, 0);
}

// Returns a root window id with a given index offset.
inline WindowId RootWindowId(uint16_t index) {
  return WindowId(kInvalidConnectionId, 2 + index);
}

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_IDS_H_
