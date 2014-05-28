// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_TEST_CHANGE_TRACKER_H_
#define MOJO_SERVICES_VIEW_MANAGER_TEST_CHANGE_TRACKER_H_

#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/services/public/cpp/view_manager/view_manager_types.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "ui/gfx/rect.h"

namespace mojo {
namespace view_manager {
namespace service {

enum ChangeType {
  CHANGE_TYPE_CONNECTION_ESTABLISHED,
  CHANGE_TYPE_SERVER_CHANGE_ID_ADVANCED,
  CHANGE_TYPE_NODE_BOUNDS_CHANGED,
  CHANGE_TYPE_NODE_HIERARCHY_CHANGED,
  CHANGE_TYPE_NODE_DELETED,
  CHANGE_TYPE_VIEW_DELETED,
  CHANGE_TYPE_VIEW_REPLACED,
};

struct TestNode {
  // Returns a string description of this.
  std::string ToString() const;

  TransportNodeId parent_id;
  TransportNodeId node_id;
  TransportNodeId view_id;
};

// Tracks a call to IViewManagerClient. See the individual functions for the
// fields that are used.
struct Change {
  Change();
  ~Change();

  ChangeType type;
  TransportConnectionId connection_id;
  TransportChangeId change_id;
  std::vector<TestNode> nodes;
  TransportNodeId node_id;
  TransportNodeId node_id2;
  TransportNodeId node_id3;
  TransportViewId view_id;
  TransportViewId view_id2;
  gfx::Rect bounds;
  gfx::Rect bounds2;
};

// Converts Changes to string descriptions.
std::vector<std::string> ChangesToDescription1(
    const std::vector<Change>& changes);

// Returns a string description of |changes[0].nodes|. Returns an empty string
// if change.size() != 1.
std::string ChangeNodeDescription(const std::vector<Change>& changes);

// TestChangeTracker is used to record IViewManagerClient functions. It notifies
// a delegate any time a change is added.
class TestChangeTracker {
 public:
  // Used to notify the delegate when a change is added. A change corresponds to
  // a single IViewManagerClient function.
  class Delegate {
   public:
    virtual void OnChangeAdded() = 0;

   protected:
    virtual ~Delegate() {}
  };

  TestChangeTracker();
  ~TestChangeTracker();

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  std::vector<Change>* changes() { return &changes_; }

  // Each of these functions generate a Change. There is one per
  // IViewManagerClient function.
  void OnViewManagerConnectionEstablished(
      TransportConnectionId connection_id,
      TransportChangeId next_server_change_id,
      const Array<INode>& nodes);
  void OnServerChangeIdAdvanced(TransportChangeId change_id);
  void OnNodeBoundsChanged(TransportNodeId node_id,
                           const Rect& old_bounds,
                           const Rect& new_bounds);
  void OnNodeHierarchyChanged(TransportNodeId node_id,
                              TransportNodeId new_parent_id,
                              TransportNodeId old_parent_id,
                              TransportChangeId server_change_id,
                              const Array<INode>& nodes);
  void OnNodeDeleted(TransportNodeId node_id,
                     TransportChangeId server_change_id);
  void OnViewDeleted(TransportViewId view_id);
  void OnNodeViewReplaced(TransportNodeId node_id,
                          TransportViewId new_view_id,
                          TransportViewId old_view_id);

 private:
  void AddChange(const Change& change);

  Delegate* delegate_;
  std::vector<Change> changes_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeTracker);
};

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_TEST_CHANGE_TRACKER_H_
