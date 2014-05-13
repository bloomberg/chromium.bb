// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"

namespace mojo {
class Shell;
namespace services {
namespace view_manager {

class ViewManagerSynchronizer;
class ViewTreeNode;

// Approximately encapsulates the View Manager service.
// Owns a synchronizer that keeps a client model in sync with the service.
// Owned by the creator.
//
// TODO: displays
class ViewManager {
 public:
  explicit ViewManager(Shell* shell);
  ~ViewManager();

  // Connects to the View Manager service. This method must be called before
  // using any other View Manager lib class or function.
  // Blocks on establishing the connection and subsequently receiving a node
  // tree from the service.
  // TODO(beng): blocking is currently achieved by running a nested runloop,
  //             which will dispatch all messages on all pipes while blocking.
  //             we should instead wait on the client pipe receiving a
  //             connection established message.
  // TODO(beng): this method could optionally not block if supplied a callback.
  void Init();

  ViewTreeNode* tree() { return tree_; }

  ViewTreeNode* GetNodeById(TransportNodeId id);

 private:
  friend class ViewManagerPrivate;
  typedef std::map<TransportNodeId, ViewTreeNode*> IdToNodeMap;

  Shell* shell_;
  scoped_ptr<ViewManagerSynchronizer> synchronizer_;
  ViewTreeNode* tree_;

  IdToNodeMap nodes_;

  DISALLOW_COPY_AND_ASSIGN(ViewManager);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
