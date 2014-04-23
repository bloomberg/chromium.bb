// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_

#include <set>

#include "base/basictypes.h"
#include "mojo/public/cpp/shell/service.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/node_delegate.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace services {
namespace view_manager {

class Node;
class RootNodeManager;

// Manages a connection from the client.
class MOJO_VIEW_MANAGER_EXPORT ViewManagerConnection
    : public ServiceConnection<ViewManager, ViewManagerConnection,
                               RootNodeManager>,
      public NodeDelegate {
 public:
  ViewManagerConnection();
  virtual ~ViewManagerConnection();

  uint16_t id() const { return id_; }

  // Invoked from Service when connection is established.
  void Initialize(
      ServiceConnector<ViewManagerConnection, RootNodeManager>* service_factory,
      ScopedMessagePipeHandle client_handle);

  // Returns the Node by id.
  Node* GetNode(const NodeId& id);

  // Notifies the client of a hierarchy change.
  void NotifyNodeHierarchyChanged(const NodeId& node,
                                  const NodeId& new_parent,
                                  const NodeId& old_parent,
                                  int32_t change_id);

 private:
  typedef std::map<uint16_t, Node*> NodeMap;

  // Overridden from ViewManager:
  virtual void CreateNode(uint16_t node_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void AddNode(uint32_t parent_id,
                       uint32_t child_id,
                       int32_t change_id,
                       const Callback<void(bool)>& callback) OVERRIDE;
  virtual void RemoveNodeFromParent(
      uint32_t node_id,
      int32_t change_id,
      const Callback<void(bool)>& callback) OVERRIDE;

  // Overriden from NodeDelegate:
  virtual void OnNodeHierarchyChanged(const NodeId& node,
                                      const NodeId& new_parent,
                                      const NodeId& old_parent) OVERRIDE;

  // Id of this connection as assigned by RootNodeManager. Assigned in
  // Initialize().
  uint16_t id_;

  NodeMap node_map_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerConnection);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_CONNECTION_H_
