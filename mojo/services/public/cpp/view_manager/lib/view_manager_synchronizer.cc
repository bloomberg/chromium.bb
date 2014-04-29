// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_private.h"

namespace mojo {
namespace services {
namespace view_manager {

class ViewManagerTransaction {
 public:
  virtual ~ViewManagerTransaction() {}

  void Commit() {
    DCHECK(!committed_);
    DoCommit();
    committed_ = true;
  }

  bool committed() const { return committed_; }
  uint32_t change_id() const { return change_id_; }

  // General callback to be used for commits to the service.
  void OnActionCompleted(bool success) {
    DCHECK(success);
    DoActionCompleted(success);
    synchronizer_->RemoveFromPendingQueue(this);
  }

 protected:
  enum TransactionType {
    // Node creation.
    TYPE_CREATE_VIEW_TREE_NODE,
    // Modifications to the hierarchy (addition of or removal of nodes from a
    // parent.)
    TYPE_HIERARCHY
  };

  ViewManagerTransaction(TransactionType transaction_type,
                         ViewManagerSynchronizer* synchronizer)
      : transaction_type_(transaction_type),
        change_id_(synchronizer->GetNextChangeId()),
        committed_(false),
        synchronizer_(synchronizer) {
  }

  // Overridden to perform transaction-specific commit actions.
  virtual void DoCommit() = 0;

  // Overridden to perform transaction-specific cleanup on commit ack from the
  // service.
  virtual void DoActionCompleted(bool success) = 0;

  IViewManager* service() { return synchronizer_->service_.get(); }

  uint32_t MakeTransportId(uint16_t id) {
    return (synchronizer_->connection_id_ << 16) | id;
  }

 private:
  const TransactionType transaction_type_;
  const uint32_t change_id_;
  bool committed_;
  ViewManagerSynchronizer* synchronizer_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerTransaction);
};

class CreateViewTreeNodeTransaction
    : public ViewManagerTransaction {
 public:
  CreateViewTreeNodeTransaction(uint16_t node_id,
                                ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_CREATE_VIEW_TREE_NODE, synchronizer),
        node_id_(node_id) {}
  virtual ~CreateViewTreeNodeTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->CreateNode(
        node_id_,
        base::Bind(&ViewManagerTransaction::OnActionCompleted,
                   base::Unretained(this)));
  }

  void DoActionCompleted(bool success) {
    // TODO(beng): Failure means we tried to create with an extant id for this
    //             connection. Figure out what to do.
  }

  const uint16_t node_id_;

  DISALLOW_COPY_AND_ASSIGN(CreateViewTreeNodeTransaction);
};

class HierarchyTransaction : public ViewManagerTransaction {
 public:
  enum HierarchyChangeType {
    TYPE_ADD,
    TYPE_REMOVE
  };
  HierarchyTransaction(HierarchyChangeType hierarchy_change_type,
                       uint16_t child_id,
                       uint16_t parent_id,
                       ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_HIERARCHY, synchronizer),
        hierarchy_change_type_(hierarchy_change_type),
        child_id_(child_id),
        parent_id_(parent_id) {}
  virtual ~HierarchyTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    switch (hierarchy_change_type_) {
      case TYPE_ADD:
        service()->AddNode(
            MakeTransportId(parent_id_),
            MakeTransportId(child_id_),
            change_id(),
            base::Bind(&ViewManagerTransaction::OnActionCompleted,
                       base::Unretained(this)));
        break;
      case TYPE_REMOVE:
        service()->RemoveNodeFromParent(
            MakeTransportId(child_id_),
            change_id(),
            base::Bind(&ViewManagerTransaction::OnActionCompleted,
                       base::Unretained(this)));
        break;
    }
  }

  void DoActionCompleted(bool success) {
    // TODO(beng): Failure means either one of the nodes specified didn't exist,
    //             or we passed the same node id for both params. Roll back?
  }

  const HierarchyChangeType hierarchy_change_type_;
  const uint16_t child_id_;
  const uint16_t parent_id_;

  DISALLOW_COPY_AND_ASSIGN(HierarchyTransaction);
};

ViewManagerSynchronizer::ViewManagerSynchronizer(ViewManager* view_manager)
    : view_manager_(view_manager),
      connected_(false),
      connection_id_(0),
      next_id_(0),
      next_change_id_(0) {
  InterfacePipe<services::view_manager::IViewManager, AnyInterface>
      view_manager_pipe;
  AllocationScope scope;
  ViewManagerPrivate(view_manager_).shell()->Connect(
      "mojo:mojo_view_manager", view_manager_pipe.handle_to_peer.Pass());
  service_.reset(view_manager_pipe.handle_to_self.Pass(), this);
}

ViewManagerSynchronizer::~ViewManagerSynchronizer() {
}

uint16_t ViewManagerSynchronizer::CreateViewTreeNode() {
  uint16_t id = next_id_++;
  pending_transactions_.push_back(new CreateViewTreeNodeTransaction(id, this));
  ScheduleSync();
  return id;
}

void ViewManagerSynchronizer::AddChild(uint16_t child_id, uint16_t parent_id) {
  pending_transactions_.push_back(
      new HierarchyTransaction(HierarchyTransaction::TYPE_ADD,
                               child_id,
                               parent_id,
                               this));
  ScheduleSync();
}

void ViewManagerSynchronizer::RemoveChild(uint16_t child_id,
                                          uint16_t parent_id) {
  pending_transactions_.push_back(
      new HierarchyTransaction(HierarchyTransaction::TYPE_REMOVE,
                               child_id,
                               parent_id,
                               this));
  ScheduleSync();
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerSynchronizer, IViewManagerClient implementation:

void ViewManagerSynchronizer::OnConnectionEstablished(uint16 connection_id) {
  connected_ = true;
  connection_id_ = connection_id;
  ScheduleSync();
}

void ViewManagerSynchronizer::OnNodeHierarchyChanged(uint32_t node,
                                                     uint32_t new_parent,
                                                     uint32_t old_parent,
                                                     uint32_t change_id) {
  if (change_id == 0) {
    // TODO(beng): Apply changes from another client.
  }
}

void ViewManagerSynchronizer::OnNodeViewReplaced(uint32_t node,
                                                 uint32_t new_view_id,
                                                 uint32_t old_view_id,
                                                 uint32_t change_id) {
  // ..
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerSynchronizer, private:

void ViewManagerSynchronizer::ScheduleSync() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ViewManagerSynchronizer::DoSync, base::Unretained(this)));
}

void ViewManagerSynchronizer::DoSync() {
  // The service connection may not be set up yet. OnConnectionEstablished()
  // will schedule another sync when it is.
  if (!connected_)
    return;

  Transactions::const_iterator it = pending_transactions_.begin();
  for (; it != pending_transactions_.end(); ++it) {
    if (!(*it)->committed())
      (*it)->Commit();
  }
}

uint32_t ViewManagerSynchronizer::GetNextChangeId() {
  // TODO(beng): deal with change id collisions? Important in the "never ack'ed
  //             change" case mentioned in OnNodeHierarchyChanged().
  // "0" is a special value passed to other connected clients, so we can't use
  // it.
  next_change_id_ = std::max(1u, ++next_change_id_);
  return next_change_id_;
}

void ViewManagerSynchronizer::RemoveFromPendingQueue(
    ViewManagerTransaction* transaction) {
  DCHECK_EQ(transaction, pending_transactions_.front());
  pending_transactions_.erase(pending_transactions_.begin());
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
