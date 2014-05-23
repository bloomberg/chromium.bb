// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/node_delegate.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace gfx {
class Rect;
}

namespace mojo {
namespace view_manager {
namespace service {

class Node;
class RootNodeManager;
class View;

#if defined(OS_WIN)
// Equivalent of NON_EXPORTED_BASE which does not work with the template snafu
// below.
#pragma warning(push)
#pragma warning(disable : 4275)
#endif

// Manages a connection from the client.
class MOJO_VIEW_MANAGER_EXPORT ViewManagerConnection
    : public InterfaceImpl<IViewManager>,
      public NodeDelegate {
 public:
  ViewManagerConnection(RootNodeManager* root_node_manager);
  virtual ~ViewManagerConnection();

  virtual void OnConnectionEstablished() MOJO_OVERRIDE;

  TransportConnectionId id() const { return id_; }

  // Returns the Node with the specified id.
  Node* GetNode(const NodeId& id) {
    return const_cast<Node*>(
        const_cast<const ViewManagerConnection*>(this)->GetNode(id));
  }
  const Node* GetNode(const NodeId& id) const;

  // Returns the View with the specified id.
  View* GetView(const ViewId& id) {
    return const_cast<View*>(
        const_cast<const ViewManagerConnection*>(this)->GetView(id));
  }
  const View* GetView(const ViewId& id) const;

  // The following methods are invoked after the corresponding change has been
  // processed. They do the appropriate bookkeeping and update the client as
  // necessary.
  void ProcessNodeBoundsChanged(const Node* node,
                                const gfx::Rect& old_bounds,
                                const gfx::Rect& new_bounds,
                                bool originated_change);
  void ProcessNodeHierarchyChanged(const Node* node,
                                   const Node* new_parent,
                                   const Node* old_parent,
                                   TransportChangeId server_change_id,
                                   bool originated_change);
  void ProcessNodeViewReplaced(const Node* node,
                               const View* new_view,
                               const View* old_view,
                               bool originated_change);
  void ProcessNodeDeleted(const NodeId& node,
                          TransportChangeId server_change_id,
                          bool originated_change);
  void ProcessViewDeleted(const ViewId& view, bool originated_change);

 private:
  typedef std::map<TransportConnectionSpecificNodeId, Node*> NodeMap;
  typedef std::map<TransportConnectionSpecificViewId, View*> ViewMap;
  typedef base::hash_set<TransportNodeId> NodeIdSet;

  // These functions return true if the corresponding mojom function is allowed
  // for this connection.
  bool CanRemoveNodeFromParent(const Node* node) const;
  bool CanAddNode(const Node* parent, const Node* child) const;
  bool CanDeleteNode(const NodeId& node_id) const;
  bool CanDeleteView(const ViewId& view_id) const;
  bool CanSetView(const Node* node, const ViewId& view_id) const;
  bool CanGetNodeTree(const Node* node) const;

  // Deletes a node owned by this connection. Returns true on success. |source|
  // is the connection that originated the change.
  bool DeleteNodeImpl(ViewManagerConnection* source, const NodeId& node_id);

  // Deletes a view owned by this connection. Returns true on success. |source|
  // is the connection that originated the change.
  bool DeleteViewImpl(ViewManagerConnection* source, const ViewId& view_id);

  // Sets the view associated with a node.
  bool SetViewImpl(Node* node, const ViewId& view_id);

  // If |node| is known (in |known_nodes_|) does nothing. Otherwise adds |node|
  // to |nodes|, marks |node| as known and recurses.
  void GetUnknownNodesFrom(const Node* node, std::vector<const Node*>* nodes);

  // Removes |node| and all its descendants from |known_nodes_|. This does not
  // recurse through nodes that were created by this connection.
  void RemoveFromKnown(const Node* node);

  // Returns true if |node| is a non-null and a descendant of |roots_| (or
  // |roots_| is empty).
  bool IsNodeDescendantOfRoots(const Node* node) const;

  // Returns true if notification should be sent of a hierarchy change. If true
  // is returned, any nodes that need to be sent to the client are added to
  // |to_send|.
  bool ShouldNotifyOnHierarchyChange(const Node* node,
                                     const Node** new_parent,
                                     const Node** old_parent,
                                     std::vector<const Node*>* to_send);

  bool ProcessSetRoots(TransportConnectionId source_connection_id,
                       const Array<TransportNodeId>& transport_node_ids);

  // Converts an array of Nodes to INodes. This assumes all the nodes are valid
  // for the client. The parent of nodes the client is not allowed to see are
  // set to NULL (in the returned INodes).
  Array<INode> NodesToINodes(const std::vector<const Node*>& nodes);

  // Overridden from IViewManager:
  virtual void CreateNode(TransportNodeId transport_node_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void DeleteNode(TransportNodeId transport_node_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void AddNode(TransportNodeId parent_id,
                       TransportNodeId child_id,
                       TransportChangeId server_change_id,
                       const Callback<void(bool)>& callback) OVERRIDE;
  virtual void RemoveNodeFromParent(
      TransportNodeId node_id,
      TransportChangeId server_change_id,
      const Callback<void(bool)>& callback) OVERRIDE;
  virtual void GetNodeTree(
      TransportNodeId node_id,
      const Callback<void(Array<INode>)>& callback) OVERRIDE;
  virtual void CreateView(TransportViewId transport_view_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void DeleteView(TransportViewId transport_view_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetView(TransportNodeId transport_node_id,
                       TransportViewId transport_view_id,
                       const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetViewContents(TransportViewId view_id,
                               ScopedSharedBufferHandle buffer,
                               uint32_t buffer_size,
                               const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetRoots(
      TransportConnectionId connection_id,
      const Array<TransportNodeId>& transport_node_ids,
      const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetNodeBounds(TransportNodeId node_id,
                             const Rect& bounds,
                             const Callback<void(bool)>& callback) OVERRIDE;

  // Overridden from NodeDelegate:
  virtual void OnNodeHierarchyChanged(const Node* node,
                                      const Node* new_parent,
                                      const Node* old_parent) OVERRIDE;
  virtual void OnNodeViewReplaced(const Node* node,
                                  const View* new_view,
                                  const View* old_view) OVERRIDE;

  RootNodeManager* root_node_manager_;

  // Id of this connection as assigned by RootNodeManager. Assigned in
  // OnConnectionEstablished().
  TransportConnectionId id_;

  NodeMap node_map_;

  ViewMap view_map_;

  // The set of nodes that has been communicated to the client.
  NodeIdSet known_nodes_;

  // This is the set of nodes the connection can parent nodes to (in addition to
  // any nodes created by this connection). If empty the connection can
  // manipulate any nodes (except for deleting other connections nodes/views).
  // The connection can not delete or move these. If this is set to a non-empty
  // value and all the nodes are deleted (by another connection), then an
  // invalid node is added here to ensure this connection is still constrained.
  NodeIdSet roots_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerConnection);
};

#if defined(OS_WIN)
#pragma warning(pop)
#endif

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_
