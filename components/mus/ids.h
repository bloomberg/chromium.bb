// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_IDS_H_
#define COMPONENTS_MUS_IDS_H_

#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/util.h"

namespace view_manager {

// Connection id is used to indicate no connection. That is, no ViewTreeImpl
// ever gets this id.
const mojo::ConnectionSpecificId kInvalidConnectionId = 0;

// Adds a bit of type safety to view ids.
struct ViewId {
  ViewId(mojo::ConnectionSpecificId connection_id,
         mojo::ConnectionSpecificId view_id)
      : connection_id(connection_id), view_id(view_id) {}
  ViewId() : connection_id(0), view_id(0) {}

  bool operator==(const ViewId& other) const {
    return other.connection_id == connection_id && other.view_id == view_id;
  }

  bool operator!=(const ViewId& other) const { return !(*this == other); }

  mojo::ConnectionSpecificId connection_id;
  mojo::ConnectionSpecificId view_id;
};

inline ViewId ViewIdFromTransportId(mojo::Id id) {
  return ViewId(mojo::HiWord(id), mojo::LoWord(id));
}

inline mojo::Id ViewIdToTransportId(const ViewId& id) {
  return (id.connection_id << 16) | id.view_id;
}

// Returns a ViewId that is reserved to indicate no view. That is, no view will
// ever be created with this id.
inline ViewId InvalidViewId() {
  return ViewId(kInvalidConnectionId, 0);
}

// Returns a root view id with a given index offset.
inline ViewId RootViewId(uint16_t index) {
  return ViewId(kInvalidConnectionId, 2 + index);
}

}  // namespace view_manager

#endif  // COMPONENTS_MUS_IDS_H_
