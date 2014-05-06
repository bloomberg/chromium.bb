// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_observer.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_private.h"
#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"
#include "mojo/services/public/cpp/view_manager/util.h"

namespace mojo {
namespace services {
namespace view_manager {

TransportNodeId MakeTransportNodeId(uint16_t connection_id,
                                    uint16_t local_node_id) {
  return (connection_id << 16) | local_node_id;
}

class ViewManagerTransaction {
 public:
  virtual ~ViewManagerTransaction() {}

  void Commit() {
    DCHECK(!committed_);
    DoCommit();
    synchronizer_->NotifyCommit();
    committed_ = true;
  }

  bool committed() const { return committed_; }
  uint32_t change_id() const { return change_id_; }

  // General callback to be used for commits to the service.
  void OnActionCompleted(bool success) {
    DCHECK(success);
    DoActionCompleted(success);
    synchronizer_->NotifyCommitResponse(success);
    synchronizer_->RemoveFromPendingQueue(this);
  }

 protected:
  enum TransactionType {
    // Node creation and destruction.
    TYPE_CREATE_VIEW_TREE_NODE,
    TYPE_DESTROY_VIEW_TREE_NODE,
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

 private:
  const TransactionType transaction_type_;
  const uint32_t change_id_;
  bool committed_;
  ViewManagerSynchronizer* synchronizer_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerTransaction);
};

class CreateViewTreeNodeTransaction : public ViewManagerTransaction {
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
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): Failure means we tried to create with an extant id for this
    //             connection. Figure out what to do.
  }

  const uint16_t node_id_;

  DISALLOW_COPY_AND_ASSIGN(CreateViewTreeNodeTransaction);
};

class DestroyViewTreeNodeTransaction : public ViewManagerTransaction {
 public:
  DestroyViewTreeNodeTransaction(TransportNodeId node_id,
                                 ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_DESTROY_VIEW_TREE_NODE, synchronizer),
        node_id_(node_id) {}
  virtual ~DestroyViewTreeNodeTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->DeleteNode(
        node_id_,
        change_id(),
        base::Bind(&ViewManagerTransaction::OnActionCompleted,
                   base::Unretained(this)));
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  TransportNodeId node_id_;
  DISALLOW_COPY_AND_ASSIGN(DestroyViewTreeNodeTransaction);
};

class HierarchyTransaction : public ViewManagerTransaction {
 public:
  enum HierarchyChangeType {
    TYPE_ADD,
    TYPE_REMOVE
  };
  HierarchyTransaction(HierarchyChangeType hierarchy_change_type,
                       TransportNodeId child_id,
                       TransportNodeId parent_id,
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
            parent_id_,
            child_id_,
            change_id(),
            base::Bind(&ViewManagerTransaction::OnActionCompleted,
                       base::Unretained(this)));
        break;
      case TYPE_REMOVE:
        service()->RemoveNodeFromParent(
            child_id_,
            change_id(),
            base::Bind(&ViewManagerTransaction::OnActionCompleted,
                       base::Unretained(this)));
        break;
    }
  }

  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): Failure means either one of the nodes specified didn't exist,
    //             or we passed the same node id for both params. Roll back?
  }

  const HierarchyChangeType hierarchy_change_type_;
  const TransportNodeId child_id_;
  const TransportNodeId parent_id_;

  DISALLOW_COPY_AND_ASSIGN(HierarchyTransaction);
};

ViewManagerSynchronizer::ViewManagerSynchronizer(ViewManager* view_manager)
    : view_manager_(view_manager),
      connected_(false),
      connection_id_(0),
      next_id_(1),
      next_change_id_(0),
      init_loop_(NULL) {
  InterfacePipe<services::view_manager::IViewManager, AnyInterface>
      view_manager_pipe;
  AllocationScope scope;
  MessagePipeHandle client_handle = view_manager_pipe.handle_to_peer.get();
  ViewManagerPrivate(view_manager_).shell()->Connect(
      "mojo:mojo_view_manager", view_manager_pipe.handle_to_peer.Pass());
  service_.reset(view_manager_pipe.handle_to_self.Pass(), this);
  base::RunLoop loop;
  init_loop_ = &loop;
  init_loop_->Run();
  init_loop_ = NULL;
}

ViewManagerSynchronizer::~ViewManagerSynchronizer() {
}

TransportNodeId ViewManagerSynchronizer::CreateViewTreeNode() {
  DCHECK(connected_);
  uint16_t id = ++next_id_;
  pending_transactions_.push_back(new CreateViewTreeNodeTransaction(id, this));
  ScheduleSync();
  return MakeTransportNodeId(connection_id_, id);
}

void ViewManagerSynchronizer::DestroyViewTreeNode(TransportNodeId node_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new DestroyViewTreeNodeTransaction(node_id, this));
  ScheduleSync();
}

void ViewManagerSynchronizer::AddChild(TransportNodeId child_id,
                                       TransportNodeId parent_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new HierarchyTransaction(HierarchyTransaction::TYPE_ADD,
                               child_id,
                               parent_id,
                               this));
  ScheduleSync();
}

void ViewManagerSynchronizer::RemoveChild(TransportNodeId child_id,
                                          TransportNodeId parent_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new HierarchyTransaction(HierarchyTransaction::TYPE_REMOVE,
                               child_id,
                               parent_id,
                               this));
  ScheduleSync();
}

void ViewManagerSynchronizer::BuildNodeTree(
    const Callback<void()>& callback) {
  DCHECK(connected_);
  service_->GetNodeTree(
      1,
      base::Bind(&ViewManagerSynchronizer::OnTreeReceived,
                 base::Unretained(this),
                 callback));
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerSynchronizer, IViewManagerClient implementation:

void ViewManagerSynchronizer::OnConnectionEstablished(uint16 connection_id) {
  connected_ = true;
  connection_id_ = connection_id;
  ScheduleSync();
  if (init_loop_)
    init_loop_->Quit();
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
  next_change_id_ = std::max(1u, next_change_id_ + 1);
  return next_change_id_;
}

void ViewManagerSynchronizer::NotifyCommit() {
  FOR_EACH_OBSERVER(ViewManagerObserver,
                    *ViewManagerPrivate(view_manager_).observers(),
                    OnCommit(view_manager_));
}

void ViewManagerSynchronizer::NotifyCommitResponse(bool success) {
  FOR_EACH_OBSERVER(ViewManagerObserver,
                    *ViewManagerPrivate(view_manager_).observers(),
                    OnCommitResponse(view_manager_, success));
}

void ViewManagerSynchronizer::RemoveFromPendingQueue(
    ViewManagerTransaction* transaction) {
  DCHECK_EQ(transaction, pending_transactions_.front());
  pending_transactions_.erase(pending_transactions_.begin());
}

void ViewManagerSynchronizer::OnTreeReceived(
    const Callback<void()>& callback,
    const Array<INode>& nodes) {
  std::vector<ViewTreeNode*> parents;
  ViewTreeNode* root = NULL;
  ViewTreeNode* last_node = NULL;
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (last_node && nodes[i].parent_id() == last_node->id()) {
      parents.push_back(last_node);
    } else if (!parents.empty()) {
      while (parents.back()->id() != nodes[i].parent_id())
        parents.pop_back();
    }
    // We don't use the ctor that takes a ViewManager here, since it will call
    // back to the service and attempt to create a new node.
    ViewTreeNode* node = new ViewTreeNode;
    ViewTreeNodePrivate private_node(node);
    private_node.set_view_manager(view_manager_);
    private_node.set_id(nodes[i].node_id());
    if (!parents.empty())
      ViewTreeNodePrivate(parents.back()).LocalAddChild(node);
    if (!last_node)
      root = node;
    last_node = node;
  }
  ViewManagerPrivate(view_manager_).set_tree(root);
  callback.Run();
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
