// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_TYPES_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_TYPES_H_

#include "base/basictypes.h"

// Typedefs for the transport types. These typedefs match that of the mojom
// file, see it for specifics.

namespace mojo {
namespace services {
namespace view_manager {

typedef uint32_t TransportChangeId;
typedef uint16_t TransportConnectionId;
typedef uint32_t TransportNodeId;
typedef uint32_t TransportViewId;

// These types are used in two cases:
// 1. when a connection is creating a node/view. In this case the connection id
//    from the current connection is used, so no need to explicitly specific it.
// 2. When the connection id is stored along side this.
typedef uint16_t TransportConnectionSpecificNodeId;
typedef uint16_t TransportConnectionSpecificViewId;

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_TYPES_H_
