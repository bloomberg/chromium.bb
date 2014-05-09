// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_SYNCHRONIZER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_SYNCHRONIZER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/services/public/cpp/view_manager/view_manager_types.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace base {
class RunLoop;
}

namespace mojo {
namespace services {
namespace view_manager {

class ViewManager;
class ViewManagerTransaction;

// Manages the connection with the View Manager service.
class ViewManagerSynchronizer : public IViewManagerClient {
 public:
  explicit ViewManagerSynchronizer(ViewManager* view_manager);
  virtual ~ViewManagerSynchronizer();

  bool connected() const { return connected_; }

  // API exposed to the node implementation that pushes local changes to the
  // service.
  TransportNodeId CreateViewTreeNode();
  void DestroyViewTreeNode(TransportNodeId node_id);

  // These methods take TransportIds. For views owned by the current connection,
  // the connection id high word can be zero. In all cases, the TransportId 0x1
  // refers to the root node.
  void AddChild(TransportNodeId child_id, TransportNodeId parent_id);
  void RemoveChild(TransportNodeId child_id, TransportNodeId parent_id);

 private:
  friend class ViewManagerTransaction;
  typedef ScopedVector<ViewManagerTransaction> Transactions;

  // Overridden from IViewManagerClient:
  virtual void OnConnectionEstablished(uint16 connection_id) OVERRIDE;
  virtual void OnNodeHierarchyChanged(uint32 node_id,
                                      uint32 new_parent_id,
                                      uint32 old_parent_id,
                                      uint32 change_id) OVERRIDE;
  virtual void OnNodeViewReplaced(uint32_t node,
                                  uint32_t new_view_id,
                                  uint32_t old_view_id,
                                  uint32_t change_id) OVERRIDE;

  // Called to schedule a sync of the client model with the service after a
  // return to the message loop.
  void ScheduleSync();

  // Sync the client model with the service by enumerating the pending
  // transaction queue and applying them in order.
  void DoSync();

  // Used by individual transactions to generate a connection-specific change
  // id.
  // TODO(beng): What happens when there are more than sizeof(int) changes in
  //             the queue?
  uint32_t GetNextChangeId();

  // Removes |transaction| from the pending queue. |transaction| must be at the
  // front of the queue.
  void RemoveFromPendingQueue(ViewManagerTransaction* transaction);

  void OnRootTreeReceived(const Array<INode>& data);

  ViewManager* view_manager_;
  bool connected_;
  uint16_t connection_id_;
  uint16_t next_id_;
  uint32_t next_change_id_;

  Transactions pending_transactions_;

  // Non-NULL while blocking on the connection to |service_| during
  // construction.
  base::RunLoop* init_loop_;

  RemotePtr<IViewManager> service_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerSynchronizer);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_SYNCHRONIZER_H_
