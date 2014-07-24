// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_NODE_OBSERVER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_NODE_OBSERVER_H_

#include <vector>

#include "base/basictypes.h"

#include "mojo/services/public/cpp/view_manager/node.h"

namespace gfx {
class Rect;
}

namespace mojo {

class Node;
class View;

// A note on -ing and -ed suffixes:
//
// -ing methods are called before changes are applied to the local node model.
// -ed methods are called after changes are applied to the local node model.
//
// If the change originated from another connection to the view manager, it's
// possible that the change has already been applied to the service-side model
// prior to being called, so for example in the case of OnNodeDestroying(), it's
// possible the node has already been destroyed on the service side.

class NodeObserver {
 public:
  struct TreeChangeParams {
    TreeChangeParams();
    Node* target;
    Node* old_parent;
    Node* new_parent;
    Node* receiver;
  };

  virtual void OnTreeChanging(const TreeChangeParams& params) {}
  virtual void OnTreeChanged(const TreeChangeParams& params) {}

  virtual void OnNodeReordering(Node* node,
                                Node* relative_node,
                                OrderDirection direction) {}
  virtual void OnNodeReordered(Node* node,
                               Node* relative_node,
                               OrderDirection direction) {}

  virtual void OnNodeDestroying(Node* node) {}
  virtual void OnNodeDestroyed(Node* node) {}

  virtual void OnNodeActiveViewChanging(Node* node,
                                        View* old_view,
                                        View* new_view) {}
  virtual void OnNodeActiveViewChanged(Node* node,
                                       View* old_view,
                                       View* new_view) {}

  virtual void OnNodeBoundsChanging(Node* node,
                                    const gfx::Rect& old_bounds,
                                    const gfx::Rect& new_bounds) {}
  virtual void OnNodeBoundsChanged(Node* node,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) {}

  virtual void OnNodeFocusChanged(Node* gained_focus, Node* lost_focus) {}

 protected:
  virtual ~NodeObserver() {}
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_NODE_OBSERVER_H_
