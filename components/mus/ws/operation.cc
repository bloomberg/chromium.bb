// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/operation.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree_impl.h"

namespace mus {
namespace ws {

Operation::Operation(WindowTreeImpl* connection,
                     ConnectionManager* connection_manager,
                     OperationType operation_type)
    : connection_manager_(connection_manager),
      source_connection_id_(connection->id()),
      operation_type_(operation_type) {
  DCHECK(operation_type != OperationType::NONE);
  // Tell the connection manager about the operation currently in flight.
  // The connection manager uses this information to suppress certain calls
  // out to clients.
  connection_manager_->PrepareForOperation(this);
}

Operation::~Operation() {
  connection_manager_->FinishOperation();
}

}  // namespace ws
}  // namespace mus
