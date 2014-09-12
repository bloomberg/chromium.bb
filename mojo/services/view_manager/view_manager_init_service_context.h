// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_CONTEXT_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_CONTEXT_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
class ApplicationConnection;

namespace service {

class ConnectionManager;
class ViewManagerInitServiceImpl;

// State shared between all ViewManagerInitService impls.
class MOJO_VIEW_MANAGER_EXPORT ViewManagerInitServiceContext {
 public:
  ViewManagerInitServiceContext();
  virtual ~ViewManagerInitServiceContext();

  void AddConnection(ViewManagerInitServiceImpl* connection);
  void RemoveConnection(ViewManagerInitServiceImpl* connection);

  void ConfigureIncomingConnection(ApplicationConnection* connection);

  void Embed(const String& url,
             ServiceProviderPtr service_provider,
             const Callback<void(bool)>& callback);

  ConnectionManager* connection_manager() { return connection_manager_.get(); }

 private:
  typedef std::vector<ViewManagerInitServiceImpl*> Connections;

  struct ConnectParams {
    ConnectParams();
    ~ConnectParams();

    std::string url;
    InterfaceRequest<ServiceProvider> service_provider;
    Callback<void(bool)> callback;
  };

  void OnNativeViewportDeleted();

  scoped_ptr<ConnectionManager> connection_manager_;
  Connections connections_;

  bool deleting_connection_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerInitServiceContext);
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_CONTEXT_H_
