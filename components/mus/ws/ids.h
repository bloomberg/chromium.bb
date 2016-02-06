// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_IDS_H_
#define COMPONENTS_MUS_WS_IDS_H_

#include <stdint.h>

#include <tuple>

#include "base/containers/hash_tables.h"
#include "base/hash.h"
#include "components/mus/common/types.h"
#include "components/mus/common/util.h"

namespace mus {
namespace ws {

// Connection id is used to indicate no connection. That is, no WindowTreeImpl
// ever gets this id.
const ConnectionSpecificId kInvalidConnectionId = 0;

// Every window has a unique id associated with it (WindowId). The id is a
// combination of the id assigned to the connection (the high order bits) and
// a unique id for the window. Each client (WindowTreeImpl) refers to the
// window by an id assigned by the client (ClientWindowId). To facilitate this
// WindowTreeImpl maintains a mapping between WindowId and ClientWindowId.
//
// This model works when the client initiates creation of windows, which is
// the typical use case. Embed roots and the WindowManager are special, they
// get access to windows created by other connections. These clients see the
// id assigned on the server. Such clients have to take care that they only
// create windows using their connection id. To do otherwise could result in
// multiple windows having the same ClientWindowId. WindowTreeImpl enforces
// that embed roots use the connection id in creating the window id to avoid
// possible conflicts.
struct WindowId {
  WindowId(ConnectionSpecificId connection_id, ConnectionSpecificId window_id)
      : connection_id(connection_id), window_id(window_id) {}
  WindowId() : connection_id(0), window_id(0) {}

  bool operator==(const WindowId& other) const {
    return other.connection_id == connection_id && other.window_id == window_id;
  }

  bool operator!=(const WindowId& other) const { return !(*this == other); }

  bool operator<(const WindowId& other) const {
    return std::tie(connection_id, window_id) <
           std::tie(other.connection_id, other.window_id);
  }

  ConnectionSpecificId connection_id;
  ConnectionSpecificId window_id;
};

// Used for ids assigned by the client.
struct ClientWindowId {
  explicit ClientWindowId(Id id) : id(id) {}
  ClientWindowId() : id(0u) {}

  bool operator==(const ClientWindowId& other) const { return other.id == id; }

  bool operator!=(const ClientWindowId& other) const {
    return !(*this == other);
  }

  bool operator<(const ClientWindowId& other) const { return id < other.id; }

  Id id;
};

inline WindowId WindowIdFromTransportId(Id id) {
  return WindowId(HiWord(id), LoWord(id));
}
inline Id WindowIdToTransportId(const WindowId& id) {
  return (id.connection_id << 16) | id.window_id;
}

// Returns a WindowId that is reserved to indicate no window. That is, no window
// will ever be created with this id.
inline WindowId InvalidWindowId() {
  return WindowId(kInvalidConnectionId, 0);
}

// Returns a root window id with a given index offset.
inline WindowId RootWindowId(uint16_t index) {
  return WindowId(kInvalidConnectionId, 2 + index);
}

}  // namespace ws
}  // namespace mus

namespace BASE_HASH_NAMESPACE {

template <>
struct hash<mus::ws::ClientWindowId> {
  size_t operator()(const mus::ws::ClientWindowId& id) const {
    return hash<mus::Id>()(id.id);
  }
};

template <>
struct hash<mus::ws::WindowId> {
  size_t operator()(const mus::ws::WindowId& id) const {
    return hash<mus::Id>()(WindowIdToTransportId(id));
  }
};

}  // namespace BASE_HASH_NAMESPACE

#endif  // COMPONENTS_MUS_WS_IDS_H_
