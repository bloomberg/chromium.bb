// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_CONTEXT_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_CONTEXT_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/services/view_manager/root_view_manager_delegate.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace service {

class RootNodeManager;
class ViewManagerInitServiceImpl;

// State shared between all ViewManagerInitService impls.
class MOJO_VIEW_MANAGER_EXPORT ViewManagerInitServiceContext
    : public RootViewManagerDelegate {
 public:
  ViewManagerInitServiceContext();
  virtual ~ViewManagerInitServiceContext();

  void AddConnection(ViewManagerInitServiceImpl* connection);
  void RemoveConnection(ViewManagerInitServiceImpl* connection);

  void ConfigureIncomingConnection(ApplicationConnection* connection);

  void Embed(const String& url,
             ServiceProviderPtr service_provider,
             const Callback<void(bool)>& callback);

  RootNodeManager* root_node_manager() { return root_node_manager_.get(); }

  bool is_tree_host_ready() const { return is_tree_host_ready_; }

 private:
  typedef std::vector<ViewManagerInitServiceImpl*> Connections;

  struct ConnectParams {
    ConnectParams();
    ~ConnectParams();

    std::string url;
    InterfaceRequest<ServiceProvider> service_provider;
    Callback<void(bool)> callback;
  };

  // RootViewManagerDelegate overrides:
  virtual void OnRootViewManagerWindowTreeHostCreated() OVERRIDE;

  void OnNativeViewportDeleted();

  void MaybeEmbed();

  scoped_ptr<RootNodeManager> root_node_manager_;
  Connections connections_;

  // Stores information about inbound calls to Embed() pending execution on
  // the window tree host being ready to use.
  ScopedVector<ConnectParams> connect_params_;

  bool is_tree_host_ready_;

  bool deleting_connection_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerInitServiceContext);
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_CONTEXT_H_
