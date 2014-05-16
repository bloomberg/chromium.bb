// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/node_delegate.h"
#include "mojo/services/view_manager/view_manager_export.h"

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
  virtual void OnConnectionError() MOJO_OVERRIDE;

  TransportConnectionId id() const { return id_; }

  // Returns the Node with the specified id.
  Node* GetNode(const NodeId& id);

  // Returns the View with the specified id.
  View* GetView(const ViewId& id);

  // Notifies the client of a hierarchy change.
  void NotifyNodeHierarchyChanged(const NodeId& node,
                                  const NodeId& new_parent,
                                  const NodeId& old_parent,
                                  TransportChangeId server_change_id);
  void NotifyNodeViewReplaced(const NodeId& node,
                              const ViewId& new_view_id,
                              const ViewId& old_view_id);
  void NotifyNodeDeleted(const NodeId& node,
                         TransportChangeId server_change_id);
  void NotifyViewDeleted(const ViewId& view);

 private:
  typedef std::map<TransportConnectionSpecificNodeId, Node*> NodeMap;
  typedef std::map<TransportConnectionSpecificViewId, View*> ViewMap;

  // Deletes a node owned by this connection. Returns true on success. |source|
  // is the connection that originated the change.
  bool DeleteNodeImpl(ViewManagerConnection* source, const NodeId& node_id);

  // Deletes a view owned by this connection. Returns true on success. |source|
  // is the connection that originated the change.
  bool DeleteViewImpl(ViewManagerConnection* source, const ViewId& view_id);

  // Sets the view associated with a node.
  bool SetViewImpl(const NodeId& node_id, const ViewId& view_id);

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
                               uint32_t buffer_size) OVERRIDE;

  // Overridden from NodeDelegate:
  virtual void OnNodeHierarchyChanged(const NodeId& node,
                                      const NodeId& new_parent,
                                      const NodeId& old_parent) OVERRIDE;
  virtual void OnNodeViewReplaced(const NodeId& node,
                                  const ViewId& new_view_id,
                                  const ViewId& old_view_id) OVERRIDE;
  RootNodeManager* root_node_manager_;

  // Id of this connection as assigned by RootNodeManager. Assigned in
  // OnConnectionEstablished().
  TransportConnectionId id_;

  NodeMap node_map_;

  ViewMap view_map_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerConnection);
};

#if defined(OS_WIN)
#pragma warning(pop)
#endif

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_
