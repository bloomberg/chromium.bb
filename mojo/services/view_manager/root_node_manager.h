// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_ROOT_NODE_MANAGER_H_
#define MOJO_SERVICES_VIEW_MANAGER_ROOT_NODE_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "mojo/services/view_manager/node.h"
#include "mojo/services/view_manager/node_delegate.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace services {
namespace view_manager {

class View;
class ViewManagerConnection;

// RootNodeManager is responsible for managing the set of ViewManagerConnections
// as well as providing the root of the node hierarchy.
class MOJO_VIEW_MANAGER_EXPORT RootNodeManager
    : public NativeViewportClient,
      public NodeDelegate {
 public:
  // Create when a ViewManagerConnection is about to make a change. Ensures
  // clients are notified of the correct change id.
  class ScopedChange {
   public:
    ScopedChange(ViewManagerConnection* connection,
                 RootNodeManager* root,
                 int32_t change_id);
    ~ScopedChange();

   private:
    RootNodeManager* root_;

    DISALLOW_COPY_AND_ASSIGN(ScopedChange);
  };

  RootNodeManager();
  virtual ~RootNodeManager();

  // Returns the id for the next ViewManagerConnection.
  uint16_t GetAndAdvanceNextConnectionId();

  void AddConnection(ViewManagerConnection* connection);
  void RemoveConnection(ViewManagerConnection* connection);

  // Returns the Node identified by |id|.
  Node* GetNode(const NodeId& id);

  // Notifies all ViewManagerConnections of a hierarchy change.
  void NotifyNodeHierarchyChanged(const NodeId& node,
                                  const NodeId& new_parent,
                                  const NodeId& old_parent);

 private:
  // Tracks a change.
  struct Change {
    Change(int32_t connection_id, int32_t change_id)
        : connection_id(connection_id),
          change_id(change_id) {
    }

    int32_t connection_id;
    int32_t change_id;
  };

  typedef std::map<uint16_t, ViewManagerConnection*> ConnectionMap;

  // Invoked when a particular connection is about to make a change. Records
  // the |change_id| so that it can be supplied to the clients by way of
  // OnNodeHierarchyChanged().
  // Changes should never nest, meaning each PrepareForChange() must be
  // balanced with a call to FinishChange() with no PrepareForChange()
  // in between.
  void PrepareForChange(ViewManagerConnection* connection, int32_t change_id);

  // Balances a call to PrepareForChange().
  void FinishChange();

  // Overridden from NativeViewportClient:
  virtual void OnCreated() OVERRIDE;
  virtual void OnDestroyed() OVERRIDE;
  virtual void OnBoundsChanged(const Rect& bounds) OVERRIDE;
  virtual void OnEvent(const Event& event,
                       const mojo::Callback<void()>& callback) OVERRIDE;

  // Overriden from NodeDelegate:
  virtual void OnNodeHierarchyChanged(const NodeId& node,
                                      const NodeId& new_parent,
                                      const NodeId& old_parent) OVERRIDE;

  // ID to use for next ViewManagerConnection.
  uint16_t next_connection_id_;

  // Set of ViewManagerConnections.
  ConnectionMap connection_map_;

  // Root node.
  Node root_;

  // If non-null we're processing a change.
  scoped_ptr<Change> change_;

  DISALLOW_COPY_AND_ASSIGN(RootNodeManager);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_ROOT_NODE_MANAGER_H_
