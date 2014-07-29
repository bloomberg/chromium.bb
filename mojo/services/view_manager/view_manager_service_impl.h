// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/access_policy_delegate.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace gfx {
class Rect;
}

namespace mojo {
namespace service {

class AccessPolicy;
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
class MOJO_VIEW_MANAGER_EXPORT ViewManagerServiceImpl
    : public InterfaceImpl<ViewManagerService>,
      public AccessPolicyDelegate {
 public:
  ViewManagerServiceImpl(RootNodeManager* root_node_manager,
                         ConnectionSpecificId creator_id,
                         const std::string& creator_url,
                         const std::string& url,
                         const NodeId& root_id);
  virtual ~ViewManagerServiceImpl();

  // Used to mark this connection as originating from a call to
  // ViewManagerService::Connect(). When set OnConnectionError() deletes |this|.
  void set_delete_on_connection_error() { delete_on_connection_error_ = true; }

  ConnectionSpecificId id() const { return id_; }
  ConnectionSpecificId creator_id() const { return creator_id_; }
  const std::string& url() const { return url_; }

  // Returns the Node with the specified id.
  Node* GetNode(const NodeId& id) {
    return const_cast<Node*>(
        const_cast<const ViewManagerServiceImpl*>(this)->GetNode(id));
  }
  const Node* GetNode(const NodeId& id) const;

  // Returns the View with the specified id.
  View* GetView(const ViewId& id) {
    return const_cast<View*>(
        const_cast<const ViewManagerServiceImpl*>(this)->GetView(id));
  }
  const View* GetView(const ViewId& id) const;

  // Returns true if this has |id| as a root.
  bool HasRoot(const NodeId& id) const;

  // Invoked when a connection is destroyed.
  void OnViewManagerServiceImplDestroyed(ConnectionSpecificId id);

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
                                   bool originated_change);
  void ProcessNodeReorder(const Node* node,
                          const Node* relative_node,
                          OrderDirection direction,
                          bool originated_change);
  void ProcessNodeViewReplaced(const Node* node,
                               const View* new_view,
                               const View* old_view,
                               bool originated_change);
  void ProcessNodeDeleted(const NodeId& node, bool originated_change);
  void ProcessViewDeleted(const ViewId& view, bool originated_change);
  void ProcessFocusChanged(const Node* focused_node,
                           const Node* blurred_node,
                           bool originated_change);

  // TODO(sky): move this to private section (currently can't because of
  // bindings).
  // InterfaceImp overrides:
  virtual void OnConnectionError() MOJO_OVERRIDE;

 private:
  typedef std::map<ConnectionSpecificId, Node*> NodeMap;
  typedef std::map<ConnectionSpecificId, View*> ViewMap;
  typedef base::hash_set<Id> NodeIdSet;

  bool IsNodeKnown(const Node* node) const;

  // These functions return true if the corresponding mojom function is allowed
  // for this connection.
  bool CanReorderNode(const Node* node,
                      const Node* relative_node,
                      OrderDirection direction) const;

  // Deletes a node owned by this connection. Returns true on success. |source|
  // is the connection that originated the change.
  bool DeleteNodeImpl(ViewManagerServiceImpl* source, Node* node);

  // Deletes a view owned by this connection. Returns true on success. |source|
  // is the connection that originated the change.
  bool DeleteViewImpl(ViewManagerServiceImpl* source, View* view);

  // Sets the view associated with a node.
  bool SetViewImpl(Node* node, View* view);

  // If |node| is known (in |known_nodes_|) does nothing. Otherwise adds |node|
  // to |nodes|, marks |node| as known and recurses.
  void GetUnknownNodesFrom(const Node* node, std::vector<const Node*>* nodes);

  // Removes |node| and all its descendants from |known_nodes_|. This does not
  // recurse through nodes that were created by this connection. All nodes owned
  // by this connection are added to |local_nodes|.
  void RemoveFromKnown(const Node* node, std::vector<Node*>* local_nodes);

  // Adds |node_id| to the set of roots this connection knows about. The caller
  // should have verified |node_id| is not among the roots of this connection.
  void AddRoot(const NodeId& node_id);

  // Removes |node_id| from the set of roots this connection knows about.
  void RemoveRoot(const NodeId& node_id);

  void RemoveChildrenAsPartOfEmbed(const NodeId& node_id);

  // Converts Node(s) to NodeData(s) for transport. This assumes all the nodes
  // are valid for the client. The parent of nodes the client is not allowed to
  // see are set to NULL (in the returned NodeData(s)).
  Array<NodeDataPtr> NodesToNodeDatas(const std::vector<const Node*>& nodes);
  NodeDataPtr NodeToNodeData(const Node* node);

  // Implementation of GetNodeTree(). Adds |node| to |nodes| and recurses if
  // CanDescendIntoNodeForNodeTree() returns true.
  void GetNodeTreeImpl(const Node* node, std::vector<const Node*>* nodes) const;

  // ViewManagerService:
  virtual void CreateNode(Id transport_node_id,
                          const Callback<void(ErrorCode)>& callback) OVERRIDE;
  virtual void DeleteNode(Id transport_node_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void AddNode(Id parent_id,
                       Id child_id,
                       const Callback<void(bool)>& callback) OVERRIDE;
  virtual void RemoveNodeFromParent(
      Id node_id,
      const Callback<void(bool)>& callback) OVERRIDE;
  virtual void ReorderNode(Id node_id,
                           Id relative_node_id,
                           OrderDirection direction,
                           const Callback<void(bool)>& callback) OVERRIDE;
  virtual void GetNodeTree(
      Id node_id,
      const Callback<void(Array<NodeDataPtr>)>& callback) OVERRIDE;
  virtual void CreateView(Id transport_view_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void DeleteView(Id transport_view_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetView(Id transport_node_id,
                       Id transport_view_id,
                       const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetViewContents(Id view_id,
                               ScopedSharedBufferHandle buffer,
                               uint32_t buffer_size,
                               const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetFocus(Id node_id,
                        const Callback<void(bool)> & callback) OVERRIDE;
  virtual void SetNodeBounds(Id node_id,
                             RectPtr bounds,
                             const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetNodeVisibility(Id transport_node_id,
                                 bool visible,
                                 const Callback<void(bool)>& callback) OVERRIDE;
  virtual void Embed(const String& url,
                     Id transport_node_id,
                     const Callback<void(bool)>& callback) OVERRIDE;
  virtual void DispatchOnViewInputEvent(Id transport_view_id,
                                        EventPtr event) OVERRIDE;

  // InterfaceImpl:
  virtual void OnConnectionEstablished() MOJO_OVERRIDE;

  // AccessPolicyDelegate:
  virtual const base::hash_set<Id>& GetRootsForAccessPolicy() const OVERRIDE;
  virtual bool IsNodeKnownForAccessPolicy(const Node* node) const OVERRIDE;
  virtual bool IsNodeRootOfAnotherConnectionForAccessPolicy(
      const Node* node) const OVERRIDE;

  RootNodeManager* root_node_manager_;

  // Id of this connection as assigned by RootNodeManager.
  const ConnectionSpecificId id_;

  // URL this connection was created for.
  const std::string url_;

  // ID of the connection that created us. If 0 it indicates either we were
  // created by the root, or the connection that created us has been destroyed.
  ConnectionSpecificId creator_id_;

  // The URL of the app that embedded the app this connection was created for.
  const std::string creator_url_;

  scoped_ptr<AccessPolicy> access_policy_;

  // The nodes and views created by this connection. This connection owns these
  // objects.
  NodeMap node_map_;
  ViewMap view_map_;

  // The set of nodes that has been communicated to the client.
  NodeIdSet known_nodes_;

  // Set of root nodes from other connections. More specifically any time
  // Embed() is invoked the id of the node is added to this set for the child
  // connection. The connection Embed() was invoked on (the parent) doesn't
  // directly track which connections are attached to which of its nodes. That
  // information can be found by looking through the |roots_| of all
  // connections.
  NodeIdSet roots_;

  // See description above setter.
  bool delete_on_connection_error_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerServiceImpl);
};

#if defined(OS_WIN)
#pragma warning(pop)
#endif

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_
