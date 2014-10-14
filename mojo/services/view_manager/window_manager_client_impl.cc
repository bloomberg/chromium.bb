// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/window_manager_client_impl.h"

#include "base/bind.h"
#include "mojo/services/view_manager/connection_manager.h"
#include "mojo/services/view_manager/view_manager_service_impl.h"

namespace mojo {
namespace service {

WindowManagerClientImpl::WindowManagerClientImpl(
    ConnectionManager* connection_manager)
    : connection_manager_(connection_manager) {
}

WindowManagerClientImpl::~WindowManagerClientImpl() {
}

void WindowManagerClientImpl::DispatchInputEventToView(Id transport_view_id,
                                                       EventPtr event) {
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));

  ViewManagerServiceImpl* connection =
      connection_manager_->GetConnectionWithRoot(view_id);
  if (!connection)
    connection = connection_manager_->GetConnection(view_id.connection_id);
  if (connection) {
    connection->client()->OnViewInputEvent(
        transport_view_id, event.Pass(), base::Bind(&base::DoNothing));
  }
}

void WindowManagerClientImpl::OnConnectionError() {
  // TODO(sky): deal with this. We may need to tear everything down here.
}

}  // namespace service
}  // namespace mojo
