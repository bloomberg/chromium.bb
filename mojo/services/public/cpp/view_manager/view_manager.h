// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"

namespace mojo {
class Shell;
namespace services {
namespace view_manager {

class ViewManagerObserver;
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

  void BuildNodeTree(const mojo::Callback<void()>& callback);

  ViewTreeNode* tree() { return tree_.get(); }

 private:
  friend class ViewManagerPrivate;

  void AddObserver(ViewManagerObserver* observer);
  void RemoveObserver(ViewManagerObserver* observer);

  Shell* shell_;
  scoped_ptr<ViewManagerSynchronizer> synchronizer_;
  scoped_ptr<ViewTreeNode> tree_;

  ObserverList<ViewManagerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ViewManager);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
