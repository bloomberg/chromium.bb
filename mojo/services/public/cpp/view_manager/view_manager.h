// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"

namespace mojo {
class Application;
namespace view_manager {

class View;
class ViewManagerDelegate;
class ViewManagerSynchronizer;
class ViewTreeNode;

// Approximately encapsulates the View Manager service.
// Has a synchronizer that keeps a client model in sync with the service.
// Owned by the connection.
class ViewManager {
 public:
  ~ViewManager();

  // Delegate is owned by the caller.
  static void Create(Application* application, ViewManagerDelegate* delegate);

  const std::vector<ViewTreeNode*>& roots() { return roots_; }

  ViewTreeNode* GetNodeById(TransportNodeId id);
  View* GetViewById(TransportViewId id);

 private:
  friend class ViewManagerPrivate;
  friend class ViewManagerSynchronizer;

  typedef std::map<TransportNodeId, ViewTreeNode*> IdToNodeMap;
  typedef std::map<TransportViewId, View*> IdToViewMap;
  typedef std::map<TransportConnectionId,
                   ViewManagerSynchronizer*> SynchronizerMap;

  ViewManager(ViewManagerSynchronizer* synchronizer,
              ViewManagerDelegate* delegate);

  ViewManagerDelegate* delegate_;
  ViewManagerSynchronizer* synchronizer_;

  std::vector<ViewTreeNode*> roots_;

  IdToNodeMap nodes_;
  IdToViewMap views_;

  DISALLOW_COPY_AND_ASSIGN(ViewManager);
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
