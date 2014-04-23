// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_NODE_H_
#define MOJO_SERVICES_VIEW_MANAGER_NODE_H_

#include "base/logging.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_export.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace mojo {
namespace services {
namespace view_manager {

class NodeDelegate;

// Represents a node in the graph. Delegate is informed of interesting events.
class MOJO_VIEW_MANAGER_EXPORT Node : public aura::WindowObserver {
 public:
  Node(NodeDelegate* delegate, const NodeId& id);
  virtual ~Node();

  void set_view_id(const ViewId& view_id) { view_id_ = view_id; }
  const ViewId& view_id() const { return view_id_; }

  const NodeId& id() const { return id_; }

  void Add(Node* child);
  void Remove(Node* child);

  Node* GetParent();

 private:
  // WindowObserver overrides:
  virtual void OnWindowHierarchyChanged(
      const aura::WindowObserver::HierarchyChangeParams& params) OVERRIDE;

  NodeDelegate* delegate_;
  const NodeId id_;
  ViewId view_id_;
  aura::Window window_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_NODE_H_
