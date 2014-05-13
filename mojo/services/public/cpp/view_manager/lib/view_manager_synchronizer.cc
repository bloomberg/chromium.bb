// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/shell/service.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
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
    committed_ = true;
  }

  bool committed() const { return committed_; }
  TransportChangeId client_change_id() const { return client_change_id_; }

  // General callback to be used for commits to the service.
  void OnActionCompleted(bool success) {
    DCHECK(success);
    DoActionCompleted(success);
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
        client_change_id_(synchronizer->GetNextClientChangeId()),
        committed_(false),
        synchronizer_(synchronizer) {
  }

  // Overridden to perform transaction-specific commit actions.
  virtual void DoCommit() = 0;

  // Overridden to perform transaction-specific cleanup on commit ack from the
  // service.
  virtual void DoActionCompleted(bool success) = 0;

  IViewManager* service() { return synchronizer_->service_.get(); }

  TransportChangeId GetAndAdvanceNextServerChangeId() {
    return synchronizer_->next_server_change_id_++;
  }

 private:
  const TransactionType transaction_type_;
  const uint32_t client_change_id_;
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
    //             connection. It also could mean we tried to do something
    //             invalid, or we tried applying a change out of order. Figure
    //             out what to do.
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
        client_change_id(),
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
            GetAndAdvanceNextServerChangeId(),
            client_change_id(),
            base::Bind(&ViewManagerTransaction::OnActionCompleted,
                       base::Unretained(this)));
        break;
      case TYPE_REMOVE:
        service()->RemoveNodeFromParent(
            child_id_,
            GetAndAdvanceNextServerChangeId(),
            client_change_id(),
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
      next_client_change_id_(0),
      next_server_change_id_(0),
      sync_factory_(this),
      init_loop_(NULL) {
  ConnectTo(ViewManagerPrivate(view_manager_).shell(), "mojo:mojo_view_manager",
            &service_);
  service_->SetClient(this);

  AllocationScope scope;
  service_->GetNodeTree(
      1,
      base::Bind(&ViewManagerSynchronizer::OnRootTreeReceived,
                 base::Unretained(this)));

  base::RunLoop loop;
  init_loop_ = &loop;
  init_loop_->Run();
  init_loop_ = NULL;
}

ViewManagerSynchronizer::~ViewManagerSynchronizer() {
  Sync();
}

TransportNodeId ViewManagerSynchronizer::CreateViewTreeNode() {
  DCHECK(connected_);
  uint16_t id = ++next_id_;
  pending_transactions_.push_back(new CreateViewTreeNodeTransaction(id, this));
  Sync();
  return MakeTransportNodeId(connection_id_, id);
}

void ViewManagerSynchronizer::DestroyViewTreeNode(TransportNodeId node_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new DestroyViewTreeNodeTransaction(node_id, this));
  Sync();
}

void ViewManagerSynchronizer::AddChild(TransportNodeId child_id,
                                       TransportNodeId parent_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new HierarchyTransaction(HierarchyTransaction::TYPE_ADD,
                               child_id,
                               parent_id,
                               this));
  Sync();
}

void ViewManagerSynchronizer::RemoveChild(TransportNodeId child_id,
                                          TransportNodeId parent_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new HierarchyTransaction(HierarchyTransaction::TYPE_REMOVE,
                               child_id,
                               parent_id,
                               this));
  Sync();
}

bool ViewManagerSynchronizer::OwnsNode(TransportNodeId id) const {
  return HiWord(id) == connection_id_;
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerSynchronizer, IViewManagerClient implementation:

void ViewManagerSynchronizer::OnConnectionEstablished(
    TransportConnectionId connection_id,
    TransportChangeId next_server_change_id) {
  connected_ = true;
  connection_id_ = connection_id;
  next_server_change_id_ = next_server_change_id;
  Sync();
}

void ViewManagerSynchronizer::OnNodeHierarchyChanged(
    uint32_t node_id,
    uint32_t new_parent_id,
    uint32_t old_parent_id,
    TransportChangeId server_change_id,
    TransportChangeId client_change_id) {
  if (client_change_id == 0) {
    // Change originated from another client.
    ViewTreeNode* new_parent =
        view_manager_->tree()->GetChildById(new_parent_id);
    ViewTreeNode* old_parent =
        view_manager_->tree()->GetChildById(old_parent_id);
    ViewTreeNode* node = NULL;
    if (old_parent) {
      // Existing node, mapped in this connection's tree.
      // TODO(beng): verify this is actually true.
      node = view_manager_->GetNodeById(node_id);
      DCHECK_EQ(node->parent(), old_parent);
    } else {
      // New node, originating from another connection.
      node = ViewTreeNodePrivate::LocalCreate();
      ViewTreeNodePrivate private_node(node);
      private_node.set_view_manager(view_manager_);
      private_node.set_id(node_id);
      ViewManagerPrivate(view_manager_).AddNode(node->id(), node);
    }
    if (new_parent)
      ViewTreeNodePrivate(new_parent).LocalAddChild(node);
    else
      ViewTreeNodePrivate(old_parent).LocalRemoveChild(node);
  }
  next_server_change_id_ = server_change_id + 1;
}

void ViewManagerSynchronizer::OnNodeDeleted(
    TransportNodeId node_id,
    TransportChangeId server_change_id,
    TransportChangeId client_change_id) {
  next_server_change_id_ = server_change_id + 1;
  if (client_change_id == 0) {
    ViewTreeNode* node = view_manager_->GetNodeById(node_id);
    if (node)
      ViewTreeNodePrivate(node).LocalDestroy();
  }
}

void ViewManagerSynchronizer::OnNodeViewReplaced(uint32_t node,
                                                 uint32_t new_view_id,
                                                 uint32_t old_view_id,
                                                 uint32_t client_change_id) {
  // ..
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerSynchronizer, private:

void ViewManagerSynchronizer::Sync() {
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

uint32_t ViewManagerSynchronizer::GetNextClientChangeId() {
  // TODO(beng): deal with change id collisions? Important in the "never ack'ed
  //             change" case mentioned in OnNodeHierarchyChanged().
  // "0" is a special value passed to other connected clients, so we can't use
  // it.
  next_client_change_id_ = std::max(1u, next_client_change_id_ + 1);
  return next_client_change_id_;
}

void ViewManagerSynchronizer::RemoveFromPendingQueue(
    ViewManagerTransaction* transaction) {
  DCHECK_EQ(transaction, pending_transactions_.front());
  pending_transactions_.erase(pending_transactions_.begin());
}

void ViewManagerSynchronizer::OnRootTreeReceived(
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
    ViewTreeNode* node = ViewTreeNodePrivate::LocalCreate();
    ViewTreeNodePrivate private_node(node);
    private_node.set_view_manager(view_manager_);
    private_node.set_id(nodes[i].node_id());
    if (!parents.empty())
      ViewTreeNodePrivate(parents.back()).LocalAddChild(node);
    if (!last_node)
      root = node;
    last_node = node;
    ViewManagerPrivate(view_manager_).AddNode(node->id(), node);
  }
  ViewManagerPrivate(view_manager_).set_root(root);
  if (init_loop_)
    init_loop_->Quit();
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
