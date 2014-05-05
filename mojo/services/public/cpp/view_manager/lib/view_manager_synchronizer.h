// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_SYNCHRONIZER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_SYNCHRONIZER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

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

  // API exposed to the node implementation that pushes local changes to the
  // service.
  uint16_t CreateViewTreeNode();
  void AddChild(uint16_t child_id, uint16_t parent_id);
  void RemoveChild(uint16_t child_id, uint16_t parent_id);

 private:
  friend class ViewManagerTransaction;
  typedef ScopedVector<ViewManagerTransaction> Transactions;

  // Overridden from IViewManagerClient:
  virtual void OnConnectionEstablished(uint16 connection_id) OVERRIDE;
  virtual void OnNodeHierarchyChanged(uint32 node,
                                      uint32 new_parent,
                                      uint32 old_parent,
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

  ViewManager* view_manager_;
  bool connected_;
  uint16_t connection_id_;
  uint16_t next_id_;
  uint32_t next_change_id_;

  Transactions pending_transactions_;

  RemotePtr<IViewManager> service_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerSynchronizer);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_SYNCHRONIZER_H_
