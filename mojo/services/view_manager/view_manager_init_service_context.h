// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_CONTEXT_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_CONTEXT_H_

#include <set>

#include "base/memory/scoped_ptr.h"
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

  RootNodeManager* root_node_manager() { return root_node_manager_.get(); }

  bool is_tree_host_ready() const { return is_tree_host_ready_; }

 private:
  typedef std::vector<ViewManagerInitServiceImpl*> Connections;

  // RootViewManagerDelegate overrides:
  virtual void OnRootViewManagerWindowTreeHostCreated() OVERRIDE;

  void OnNativeViewportDeleted();

  scoped_ptr<RootNodeManager> root_node_manager_;
  Connections connections_;

  bool is_tree_host_ready_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerInitServiceContext);
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_CONTEXT_H_
