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
                 RootNodeManager::ChangeType change_type,
                 bool is_delete_node);
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

  bool IsProcessingChange() const { return change_source_ != 0; }

  bool is_processing_delete_node() const { return is_processing_delete_node_; }

  // These functions trivially delegate to all ViewManagerConnections, which in
  // term notify their clients.
  void ProcessNodeBoundsChanged(const Node* node,
                                const gfx::Rect& old_bounds,
                                const gfx::Rect& new_bounds);
  void ProcessNodeHierarchyChanged(const Node* node,
                                   const Node* new_parent,
                                   const Node* old_parent);
  void ProcessNodeViewReplaced(const Node* node,
                               const View* new_view_id,
                               const View* old_view_id);
  void ProcessNodeDeleted(const NodeId& node);
  void ProcessViewDeleted(const ViewId& view);

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
  void PrepareForChange(ViewManagerConnection* connection, bool is_delete_node);

  // Balances a call to PrepareForChange().
  void FinishChange(ChangeType change_type);

  // Returns true if the specified connection originated the current change.
  bool IsChangeSource(TransportConnectionId connection_id) const {
    return connection_id == change_source_;
  }

  // Overridden from NodeDelegate:
  virtual void OnNodeHierarchyChanged(const Node* node,
                                      const Node* new_parent,
                                      const Node* old_parent) OVERRIDE;
  virtual void OnNodeViewReplaced(const Node* node,
                                  const View* new_view,
                                  const View* old_view) OVERRIDE;

  Context context_;

  // ID to use for next ViewManagerConnection.
  TransportConnectionId next_connection_id_;

  TransportChangeId next_server_change_id_;

  // Set of ViewManagerConnections.
  ConnectionMap connection_map_;

  // If non-zero we're processing a change from this client.
  TransportConnectionId change_source_;

  // True if we're processing a DeleteNode request.
  bool is_processing_delete_node_;

  RootViewManager root_view_manager_;

  // Root node.
  Node root_;

  DISALLOW_COPY_AND_ASSIGN(RootNodeManager);
};

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_ROOT_NODE_MANAGER_H_
