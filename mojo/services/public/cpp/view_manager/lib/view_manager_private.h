// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_PRIVATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_PRIVATE_H_

#include "base/basictypes.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"

namespace mojo {
namespace services {
namespace view_manager {

class ViewManagerSynchronizer;

class ViewManagerPrivate {
 public:
  explicit ViewManagerPrivate(ViewManager* manager);
  ~ViewManagerPrivate();

  ViewManagerSynchronizer* synchronizer() {
    return manager_->synchronizer_.get();
  }
  Shell* shell() { return manager_->shell_; }

  void set_root(ViewTreeNode* root) { manager_->tree_ = root; }

  void AddNode(TransportNodeId node_id, ViewTreeNode* node);
  void RemoveNode(TransportNodeId node_id);

  // Returns true if the ViewManager's synchronizer is connected to the service.
  bool connected() { return manager_->synchronizer_->connected(); }

 private:
  ViewManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerPrivate);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_PRIVATE_H_
