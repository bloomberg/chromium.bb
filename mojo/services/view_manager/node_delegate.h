// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_NODE_DELEGATE_H_
#define MOJO_SERVICES_VIEW_MANAGER_NODE_DELEGATE_H_

#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace services {
namespace view_manager {

struct NodeId;
struct ViewId;

class MOJO_VIEW_MANAGER_EXPORT NodeDelegate {
 public:
  // Invoked when the hierarchy has changed.
  virtual void OnNodeHierarchyChanged(const NodeId& node,
                                      const NodeId& new_parent,
                                      const NodeId& old_parent) = 0;

  // Invoked when the View associated with a node changes.
  virtual void OnNodeViewReplaced(const NodeId& node,
                                  const ViewId& new_view_id,
                                  const ViewId& old_view_id) = 0;

 protected:
  virtual ~NodeDelegate() {}
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_NODE_DELEGATE_H_
