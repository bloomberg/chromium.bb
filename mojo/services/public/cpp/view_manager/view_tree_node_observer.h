// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_TREE_NODE_OBSERVER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_TREE_NODE_OBSERVER_H_

#include <vector>

#include "base/basictypes.h"

namespace mojo {
namespace services {
namespace view_manager {

class ViewTreeNode;

class ViewTreeNodeObserver {
 public:
  enum DispositionChangePhase {
    DISPOSITION_CHANGING,
    DISPOSITION_CHANGED
  };

  struct TreeChangeParams {
    TreeChangeParams();
    ViewTreeNode* target;
    ViewTreeNode* old_parent;
    ViewTreeNode* new_parent;
    ViewTreeNode* receiver;
    DispositionChangePhase phase;
  };

  virtual void OnTreeChange(const TreeChangeParams& params) {}

 protected:
  virtual ~ViewTreeNodeObserver() {}
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_TREE_NODE_OBSERVER_H_
