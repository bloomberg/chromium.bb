// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_ROOT_NODE_MANAGER_H_
#define MOJO_SERVICES_VIEW_MANAGER_ROOT_NODE_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/node.h"
#include "mojo/services/view_manager/node_delegate.h"
#include "mojo/services/view_manager/root_view_manager.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {

class Shell;

namespace view_manager {
namespace service {

class View;
class ViewManagerConnection;

// RootNodeManager is responsible for managing the set of ViewManagerConnections
// as well as providing the root of the node hierarchy.
class MOJO_VIEW_MANAGER_EXPORT RootNodeManager : public NodeDelegate {
 public:
  // Used to indicate if the server id should be incremented after notifiying
  // clients of the change.
  enum ChangeType {
    CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID,
    CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID,
  };

  // Create when a ViewManagerConnection is about to make a change. Ensures
  // clients are notified of the correct change id.
  class ScopedChange {
   public:
    ScopedChange(ViewManagerConnection* connection,
                 RootNodeManager* root,
                 RootNodeManager::ChangeType change_type);
    ~ScopedChange();

   private:
    RootNodeManager* root_;
    const ChangeType change_type_;

    DISALLOW_COPY_AND_ASSIGN(ScopedChange);
  };

  explicit RootNodeManager(Shell* shell);
  virtual ~RootNodeManager();

  // Returns the id for the next ViewManagerConnection.
  TransportConnectionId GetAndAdvanceNextConnectionId();

  TransportChangeId next_server_change_id() const {
    return next_server_change_id_;
  }

  void AddConnection(ViewManagerConnection* connection);
  void RemoveConnection(ViewManagerConnection* connection);

  // Returns the connection by id.
  ViewManagerConnection* GetConnection(TransportConnectionId connection_id);

  // Returns the Node identified by |id|.
  Node* GetNode(const NodeId& id);

  // Returns the View identified by |id|.
  View* GetView(const ViewId& id);

  Node* root() { return &root_; }

  // These functions trivially delegate to all ViewManagerConnections, which in
  // term notify their clients.
  void NotifyNodeHierarchyChanged(const NodeId& node,
                                  const NodeId& new_parent,
                                  const NodeId& old_parent);
  void NotifyNodeViewReplaced(const NodeId& node,
                              const ViewId& new_view_id,
                              const ViewId& old_view_id);
  void NotifyNodeDeleted(const NodeId& node);
  void NotifyViewDeleted(const ViewId& view);

 private:
  // Used to setup any static state needed by RootNodeManager.
  struct Context {
    Context();
    ~Context();
  };

  typedef std::map<TransportConnectionId, ViewManagerConnection*> ConnectionMap;

  // Invoked when a particular connection is about to make a change.
  // Subsequently followed by FinishChange() once the change is done.
  //
  // Changes should never nest, meaning each PrepareForChange() must be
  // balanced with a call to FinishChange() with no PrepareForChange()
  // in between.
  void PrepareForChange(ViewManagerConnection* connection);

  // Balances a call to PrepareForChange().
  void FinishChange(ChangeType change_type);

  // Returns true if the specified connection should be notified of the current
  // change.
  bool ShouldNotifyConnection(TransportConnectionId connection_id) const;

  // Overriden from NodeDelegate:
  virtual void OnNodeHierarchyChanged(const NodeId& node,
                                      const NodeId& new_parent,
                                      const NodeId& old_parent) OVERRIDE;
  virtual void OnNodeViewReplaced(const NodeId& node,
                                  const ViewId& new_view_id,
                                  const ViewId& old_view_id) OVERRIDE;

  Context context_;

  // ID to use for next ViewManagerConnection.
  TransportConnectionId next_connection_id_;

  TransportChangeId next_server_change_id_;

  // Set of ViewManagerConnections.
  ConnectionMap connection_map_;

  // If non-zero we're processing a change from this client.
  TransportConnectionId change_source_;

  RootViewManager root_view_manager_;

  // Root node.
  Node root_;

  DISALLOW_COPY_AND_ASSIGN(RootNodeManager);
};

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_ROOT_NODE_MANAGER_H_
