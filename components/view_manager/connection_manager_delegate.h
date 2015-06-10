// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_CONNECTION_MANAGER_DELEGATE_H_
#define COMPONENTS_VIEW_MANAGER_CONNECTION_MANAGER_DELEGATE_H_

#include <string>

#include "components/view_manager/public/cpp/types.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace mojo {
class ViewManagerService;
}

namespace view_manager {

class ClientConnection;
class ConnectionManager;
struct ViewId;

class ConnectionManagerDelegate {
 public:
  virtual void OnLostConnectionToWindowManager() = 0;

  // Creates a ClientConnection in response to Embed() calls on the
  // ConnectionManager.
  virtual ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
      mojo::ConnectionSpecificId creator_id,
      mojo::URLRequestPtr request,
      const ViewId& root_id) = 0;
  virtual ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
      mojo::ConnectionSpecificId creator_id,
      const ViewId& root_id,
      mojo::ViewManagerClientPtr view_manager_client) = 0;

 protected:
  virtual ~ConnectionManagerDelegate() {}
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_CONNECTION_MANAGER_DELEGATE_H_
