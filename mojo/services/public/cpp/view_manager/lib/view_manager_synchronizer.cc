// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/shell/connect.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_private.h"
#include "mojo/services/public/cpp/view_manager/lib/view_private.h"
#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {
namespace view_manager {

uint32_t MakeTransportId(uint16_t connection_id, uint16_t local_id) {
  return (connection_id << 16) | local_id;
}

// Helper called to construct a local node/view object from transport data.
ViewTreeNode* AddNodeToViewManager(ViewManager* manager,
                                   ViewTreeNode* parent,
                                   TransportNodeId node_id,
                                   TransportViewId view_id) {
  // We don't use the ctor that takes a ViewManager here, since it will call
  // back to the service and attempt to create a new node.
  ViewTreeNode* node = ViewTreeNodePrivate::LocalCreate();
  ViewTreeNodePrivate private_node(node);
  private_node.set_view_manager(manager);
  private_node.set_id(node_id);
  if (parent)
    ViewTreeNodePrivate(parent).LocalAddChild(node);
  ViewManagerPrivate private_manager(manager);
  private_manager.AddNode(node->id(), node);

  // View.
  if (view_id != 0) {
    View* view = ViewPrivate::LocalCreate();
    ViewPrivate private_view(view);
    private_view.set_view_manager(manager);
    private_view.set_id(view_id);
    private_view.set_node(node);
    // TODO(beng): this broadcasts notifications locally... do we want this? I
    //             don't think so. same story for LocalAddChild above!
    private_node.LocalSetActiveView(view);
    private_manager.AddView(view->id(), view);
  }
  return node;
}

ViewTreeNode* BuildNodeTree(ViewManager* manager,
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
    ViewTreeNode* node = AddNodeToViewManager(
        manager,
        !parents.empty() ? parents.back() : NULL,
        nodes[i].node_id(),
        nodes[i].view_id());
    if (!last_node)
      root = node;
    last_node = node;
  }
  return root;
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

 protected:
  enum TransactionType {
    // View creation and destruction.
    TYPE_CREATE_VIEW,
    TYPE_DESTROY_VIEW,
    // Node creation and destruction.
    TYPE_CREATE_VIEW_TREE_NODE,
    TYPE_DESTROY_VIEW_TREE_NODE,
    // Modifications to the hierarchy (addition of or removal of nodes from a
    // parent.)
    TYPE_HIERARCHY,
    // View replacement.
    TYPE_SET_ACTIVE_VIEW,
    // Node bounds.
    TYPE_SET_BOUNDS,
    // View contents
    TYPE_SET_VIEW_CONTENTS
  };

  ViewManagerTransaction(TransactionType transaction_type,
                         ViewManagerSynchronizer* synchronizer)
      : transaction_type_(transaction_type),
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

  base::Callback<void(bool)> ActionCompletedCallback() {
    return base::Bind(&ViewManagerTransaction::OnActionCompleted,
                      base::Unretained(this));
  }

 private:
  // General callback to be used for commits to the service.
  void OnActionCompleted(bool success) {
    DCHECK(success);
    DoActionCompleted(success);
    synchronizer_->RemoveFromPendingQueue(this);
  }

  const TransactionType transaction_type_;
  bool committed_;
  ViewManagerSynchronizer* synchronizer_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerTransaction);
};

class CreateViewTransaction : public ViewManagerTransaction {
 public:
  CreateViewTransaction(TransportViewId view_id,
                        ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_CREATE_VIEW, synchronizer),
        view_id_(view_id) {}
  virtual ~CreateViewTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->CreateView(view_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): failure.
  }

  const TransportViewId view_id_;

  DISALLOW_COPY_AND_ASSIGN(CreateViewTransaction);
};

class DestroyViewTransaction : public ViewManagerTransaction {
 public:
  DestroyViewTransaction(TransportViewId view_id,
                         ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_DESTROY_VIEW, synchronizer),
        view_id_(view_id) {}
  virtual ~DestroyViewTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->DeleteView(view_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const TransportViewId view_id_;

  DISALLOW_COPY_AND_ASSIGN(DestroyViewTransaction);
};

class CreateViewTreeNodeTransaction : public ViewManagerTransaction {
 public:
  CreateViewTreeNodeTransaction(TransportNodeId node_id,
                                ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_CREATE_VIEW_TREE_NODE, synchronizer),
        node_id_(node_id) {}
  virtual ~CreateViewTreeNodeTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->CreateNode(node_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): Failure means we tried to create with an extant id for this
    //             connection. It also could mean we tried to do something
    //             invalid, or we tried applying a change out of order. Figure
    //             out what to do.
  }

  const TransportNodeId node_id_;

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
    service()->DeleteNode(node_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const TransportNodeId node_id_;
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
            ActionCompletedCallback());
        break;
      case TYPE_REMOVE:
        service()->RemoveNodeFromParent(
            child_id_,
            GetAndAdvanceNextServerChangeId(),
            ActionCompletedCallback());
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

class SetActiveViewTransaction : public ViewManagerTransaction {
 public:
  SetActiveViewTransaction(TransportNodeId node_id,
                           TransportViewId view_id,
                           ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_SET_ACTIVE_VIEW, synchronizer),
        node_id_(node_id),
        view_id_(view_id) {}
  virtual ~SetActiveViewTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->SetView(node_id_, view_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const TransportNodeId node_id_;
  const TransportViewId view_id_;

  DISALLOW_COPY_AND_ASSIGN(SetActiveViewTransaction);
};

class SetBoundsTransaction : public ViewManagerTransaction {
 public:
  SetBoundsTransaction(TransportNodeId node_id,
                       const gfx::Rect& bounds,
                       ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_SET_BOUNDS, synchronizer),
        node_id_(node_id),
        bounds_(bounds) {}
  virtual ~SetBoundsTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    AllocationScope scope;
    service()->SetNodeBounds(node_id_, bounds_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const TransportNodeId node_id_;
  const gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(SetBoundsTransaction);
};

class SetViewContentsTransaction : public ViewManagerTransaction {
 public:
  SetViewContentsTransaction(TransportViewId view_id,
                             const SkBitmap& contents,
                             ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_SET_VIEW_CONTENTS, synchronizer),
        view_id_(view_id),
        contents_(contents) {}
  virtual ~SetViewContentsTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    std::vector<unsigned char> data;
    gfx::PNGCodec::EncodeBGRASkBitmap(contents_, false, &data);

    void* memory = NULL;
    ScopedSharedBufferHandle duped;
    bool result = CreateMapAndDupSharedBuffer(data.size(),
                                              &memory,
                                              &shared_state_handle_,
                                              &duped);
    if (!result)
      return;

    memcpy(memory, &data[0], data.size());

    AllocationScope scope;
    service()->SetViewContents(view_id_, duped.Pass(),
                               static_cast<uint32_t>(data.size()),
                               ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  bool CreateMapAndDupSharedBuffer(size_t size,
                                   void** memory,
                                   ScopedSharedBufferHandle* handle,
                                   ScopedSharedBufferHandle* duped) {
    MojoResult result = CreateSharedBuffer(NULL, size, handle);
    if (result != MOJO_RESULT_OK)
      return false;
    DCHECK(handle->is_valid());

    result = DuplicateBuffer(handle->get(), NULL, duped);
    if (result != MOJO_RESULT_OK)
      return false;
    DCHECK(duped->is_valid());

    result = MapBuffer(
        handle->get(), 0, size, memory, MOJO_MAP_BUFFER_FLAG_NONE);
    if (result != MOJO_RESULT_OK)
      return false;
    DCHECK(*memory);

    return true;
  }

  const TransportViewId view_id_;
  const SkBitmap contents_;
  ScopedSharedBufferHandle shared_state_handle_;

  DISALLOW_COPY_AND_ASSIGN(SetViewContentsTransaction);
};

ViewManagerSynchronizer::ViewManagerSynchronizer(ViewManager* view_manager)
    : view_manager_(view_manager),
      connected_(false),
      connection_id_(0),
      next_id_(1),
      next_server_change_id_(0),
      sync_factory_(this),
      init_loop_(NULL) {
  ConnectTo(ViewManagerPrivate(view_manager_).shell(), "mojo:mojo_view_manager",
            &service_);
  service_.set_client(this);

  // Start a runloop. This loop is quit when the server tells us about the
  // connection (OnConnectionEstablished()).
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
  const TransportNodeId node_id(
      MakeTransportId(connection_id_, ++next_id_));
  pending_transactions_.push_back(
      new CreateViewTreeNodeTransaction(node_id, this));
  Sync();
  return node_id;
}

void ViewManagerSynchronizer::DestroyViewTreeNode(TransportNodeId node_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new DestroyViewTreeNodeTransaction(node_id, this));
  Sync();
}

TransportViewId ViewManagerSynchronizer::CreateView() {
  DCHECK(connected_);
  const TransportNodeId view_id(
      MakeTransportId(connection_id_, ++next_id_));
  pending_transactions_.push_back(new CreateViewTransaction(view_id, this));
  Sync();
  return view_id;
}

void ViewManagerSynchronizer::DestroyView(TransportViewId view_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(new DestroyViewTransaction(view_id, this));
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

bool ViewManagerSynchronizer::OwnsView(TransportViewId id) const {
  return HiWord(id) == connection_id_;
}

void ViewManagerSynchronizer::SetActiveView(TransportNodeId node_id,
                                            TransportViewId view_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new SetActiveViewTransaction(node_id, view_id, this));
  Sync();
}

void ViewManagerSynchronizer::SetBounds(TransportNodeId node_id,
                                        const gfx::Rect& bounds) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new SetBoundsTransaction(node_id, bounds, this));
  Sync();
}

void ViewManagerSynchronizer::SetViewContents(TransportViewId view_id,
                                              const SkBitmap& contents) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new SetViewContentsTransaction(view_id, contents, this));
  Sync();
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerSynchronizer, IViewManagerClient implementation:

void ViewManagerSynchronizer::OnViewManagerConnectionEstablished(
    TransportConnectionId connection_id,
    TransportChangeId next_server_change_id,
    const Array<INode>& nodes) {
  connected_ = true;
  connection_id_ = connection_id;
  next_server_change_id_ = next_server_change_id;

  ViewManagerPrivate(view_manager_).set_root(
      BuildNodeTree(view_manager_, nodes));
  if (init_loop_)
    init_loop_->Quit();

  Sync();
}

void ViewManagerSynchronizer::OnServerChangeIdAdvanced(
    uint32_t next_server_change_id) {
  next_server_change_id_ = next_server_change_id;
}

void ViewManagerSynchronizer::OnNodeBoundsChanged(uint32 node_id,
                                                  const Rect& old_bounds,
                                                  const Rect& new_bounds) {
  ViewTreeNode* node = view_manager_->GetNodeById(node_id);
  ViewTreeNodePrivate(node).LocalSetBounds(old_bounds, new_bounds);
}

void ViewManagerSynchronizer::OnNodeHierarchyChanged(
    uint32_t node_id,
    uint32_t new_parent_id,
    uint32_t old_parent_id,
    TransportChangeId server_change_id,
    const Array<INode>& nodes) {
  // TODO: deal with |nodes|.
  next_server_change_id_ = server_change_id + 1;

  BuildNodeTree(view_manager_, nodes);

  ViewTreeNode* new_parent = view_manager_->GetNodeById(new_parent_id);
  ViewTreeNode* old_parent = view_manager_->GetNodeById(old_parent_id);
  ViewTreeNode* node = view_manager_->GetNodeById(node_id);
  if (new_parent)
    ViewTreeNodePrivate(new_parent).LocalAddChild(node);
  else
    ViewTreeNodePrivate(old_parent).LocalRemoveChild(node);
}

void ViewManagerSynchronizer::OnNodeDeleted(uint32_t node_id,
                                            uint32_t server_change_id) {
  next_server_change_id_ = server_change_id + 1;

  ViewTreeNode* node = view_manager_->GetNodeById(node_id);
  if (node)
    ViewTreeNodePrivate(node).LocalDestroy();
}

void ViewManagerSynchronizer::OnNodeViewReplaced(uint32_t node_id,
                                                 uint32_t new_view_id,
                                                 uint32_t old_view_id) {
  ViewTreeNode* node = view_manager_->GetNodeById(node_id);
  View* new_view = view_manager_->GetViewById(new_view_id);
  if (!new_view && new_view_id != 0) {
    // This client wasn't aware of this View until now.
    new_view = ViewPrivate::LocalCreate();
    ViewPrivate private_view(new_view);
    private_view.set_view_manager(view_manager_);
    private_view.set_id(new_view_id);
    private_view.set_node(node);
    ViewManagerPrivate(view_manager_).AddView(new_view->id(), new_view);
  }
  View* old_view = view_manager_->GetViewById(old_view_id);
  DCHECK_EQ(old_view, node->active_view());
  ViewTreeNodePrivate(node).LocalSetActiveView(new_view);
}

void ViewManagerSynchronizer::OnViewDeleted(uint32_t view_id) {
  View* view = view_manager_->GetViewById(view_id);
  if (view)
    ViewPrivate(view).LocalDestroy();
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

void ViewManagerSynchronizer::RemoveFromPendingQueue(
    ViewManagerTransaction* transaction) {
  DCHECK_EQ(transaction, pending_transactions_.front());
  pending_transactions_.erase(pending_transactions_.begin());
  if (pending_transactions_.empty() && !changes_acked_callback_.is_null())
    changes_acked_callback_.Run();
}

}  // namespace view_manager
}  // namespace mojo
