// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/public/cpp/view_manager/lib/view_private.h"
#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {
namespace view_manager {

Id MakeTransportId(ConnectionSpecificId connection_id,
                   ConnectionSpecificId local_id) {
  return (connection_id << 16) | local_id;
}

// Helper called to construct a local node/view object from transport data.
ViewTreeNode* AddNodeToViewManager(ViewManagerSynchronizer* synchronizer,
                                   ViewTreeNode* parent,
                                   Id node_id,
                                   Id view_id,
                                   const gfx::Rect& bounds) {
  // We don't use the ctor that takes a ViewManager here, since it will call
  // back to the service and attempt to create a new node.
  ViewTreeNode* node = ViewTreeNodePrivate::LocalCreate();
  ViewTreeNodePrivate private_node(node);
  private_node.set_view_manager(synchronizer);
  private_node.set_id(node_id);
  private_node.LocalSetBounds(gfx::Rect(), bounds);
  if (parent)
    ViewTreeNodePrivate(parent).LocalAddChild(node);
  synchronizer->AddNode(node);

  // View.
  if (view_id != 0) {
    View* view = ViewPrivate::LocalCreate();
    ViewPrivate private_view(view);
    private_view.set_view_manager(synchronizer);
    private_view.set_id(view_id);
    private_view.set_node(node);
    // TODO(beng): this broadcasts notifications locally... do we want this? I
    //             don't think so. same story for LocalAddChild above!
    private_node.LocalSetActiveView(view);
    synchronizer->AddView(view);
  }
  return node;
}

ViewTreeNode* BuildNodeTree(ViewManagerSynchronizer* synchronizer,
                            const Array<INodePtr>& nodes) {
  std::vector<ViewTreeNode*> parents;
  ViewTreeNode* root = NULL;
  ViewTreeNode* last_node = NULL;
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (last_node && nodes[i]->parent_id == last_node->id()) {
      parents.push_back(last_node);
    } else if (!parents.empty()) {
      while (parents.back()->id() != nodes[i]->parent_id)
        parents.pop_back();
    }
    ViewTreeNode* node = AddNodeToViewManager(
        synchronizer,
        !parents.empty() ? parents.back() : NULL,
        nodes[i]->node_id,
        nodes[i]->view_id,
        nodes[i]->bounds.To<gfx::Rect>());
    if (!last_node)
      root = node;
    last_node = node;
  }
  return root;
}

// Responsible for removing a root from the ViewManager when that node is
// destroyed.
class RootObserver : public ViewTreeNodeObserver {
 public:
  explicit RootObserver(ViewTreeNode* root) : root_(root) {}
  virtual ~RootObserver() {}

 private:
  // Overridden from ViewTreeNodeObserver:
  virtual void OnNodeDestroy(ViewTreeNode* node,
                             DispositionChangePhase phase) OVERRIDE {
    DCHECK_EQ(node, root_);
    if (phase != ViewTreeNodeObserver::DISPOSITION_CHANGED)
      return;
    static_cast<ViewManagerSynchronizer*>(
        ViewTreeNodePrivate(root_).view_manager())->RemoveRoot(root_);
    delete this;
  }

  ViewTreeNode* root_;

  DISALLOW_COPY_AND_ASSIGN(RootObserver);
};

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
    TYPE_SET_VIEW_CONTENTS,
    // Embed.
    TYPE_EMBED
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

  IViewManager* service() { return synchronizer_->service_; }

  Id GetAndAdvanceNextServerChangeId() {
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
  CreateViewTransaction(Id view_id, ViewManagerSynchronizer* synchronizer)
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

  const Id view_id_;

  DISALLOW_COPY_AND_ASSIGN(CreateViewTransaction);
};

class DestroyViewTransaction : public ViewManagerTransaction {
 public:
  DestroyViewTransaction(Id view_id, ViewManagerSynchronizer* synchronizer)
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

  const Id view_id_;

  DISALLOW_COPY_AND_ASSIGN(DestroyViewTransaction);
};

class CreateViewTreeNodeTransaction : public ViewManagerTransaction {
 public:
  CreateViewTreeNodeTransaction(Id node_id,
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

  const Id node_id_;

  DISALLOW_COPY_AND_ASSIGN(CreateViewTreeNodeTransaction);
};

class DestroyViewTreeNodeTransaction : public ViewManagerTransaction {
 public:
  DestroyViewTreeNodeTransaction(Id node_id,
                                 ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_DESTROY_VIEW_TREE_NODE, synchronizer),
        node_id_(node_id) {}
  virtual ~DestroyViewTreeNodeTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    GetAndAdvanceNextServerChangeId();
    service()->DeleteNode(node_id_, ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const Id node_id_;
  DISALLOW_COPY_AND_ASSIGN(DestroyViewTreeNodeTransaction);
};

class HierarchyTransaction : public ViewManagerTransaction {
 public:
  enum HierarchyChangeType {
    TYPE_ADD,
    TYPE_REMOVE
  };
  HierarchyTransaction(HierarchyChangeType hierarchy_change_type,
                       Id child_id,
                       Id parent_id,
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
  const Id child_id_;
  const Id parent_id_;

  DISALLOW_COPY_AND_ASSIGN(HierarchyTransaction);
};

class SetActiveViewTransaction : public ViewManagerTransaction {
 public:
  SetActiveViewTransaction(Id node_id,
                           Id view_id,
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

  const Id node_id_;
  const Id view_id_;

  DISALLOW_COPY_AND_ASSIGN(SetActiveViewTransaction);
};

class SetBoundsTransaction : public ViewManagerTransaction {
 public:
  SetBoundsTransaction(Id node_id,
                       const gfx::Rect& bounds,
                       ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_SET_BOUNDS, synchronizer),
        node_id_(node_id),
        bounds_(bounds) {}
  virtual ~SetBoundsTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    service()->SetNodeBounds(
        node_id_, Rect::From(bounds_), ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const Id node_id_;
  const gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(SetBoundsTransaction);
};

class SetViewContentsTransaction : public ViewManagerTransaction {
 public:
  SetViewContentsTransaction(Id view_id,
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

  const Id view_id_;
  const SkBitmap contents_;
  ScopedSharedBufferHandle shared_state_handle_;

  DISALLOW_COPY_AND_ASSIGN(SetViewContentsTransaction);
};

class EmbedTransaction : public ViewManagerTransaction {
 public:
  EmbedTransaction(const String& url,
                   Id node_id,
                   ViewManagerSynchronizer* synchronizer)
      : ViewManagerTransaction(TYPE_EMBED, synchronizer),
        url_(url),
        node_id_(node_id) {}
  virtual ~EmbedTransaction() {}

 private:
  // Overridden from ViewManagerTransaction:
  virtual void DoCommit() OVERRIDE {
    std::vector<Id> ids;
    ids.push_back(node_id_);
    service()->Connect(url_, Array<Id>::From(ids),
                       ActionCompletedCallback());
  }
  virtual void DoActionCompleted(bool success) OVERRIDE {
    // TODO(beng): recovery?
  }

  const String url_;
  const Id node_id_;

  DISALLOW_COPY_AND_ASSIGN(EmbedTransaction);
};

ViewManagerSynchronizer::ViewManagerSynchronizer(ViewManagerDelegate* delegate)
    : connected_(false),
      connection_id_(0),
      next_id_(1),
      next_server_change_id_(0),
      delegate_(delegate) {}

ViewManagerSynchronizer::~ViewManagerSynchronizer() {
  while (!nodes_.empty()) {
    IdToNodeMap::iterator it = nodes_.begin();
    if (OwnsNode(it->second->id()))
      it->second->Destroy();
    else
      nodes_.erase(it);
  }
  while (!views_.empty()) {
    IdToViewMap::iterator it = views_.begin();
    if (OwnsView(it->second->id()))
      it->second->Destroy();
    else
      views_.erase(it);
  }
}

Id ViewManagerSynchronizer::CreateViewTreeNode() {
  DCHECK(connected_);
  const Id node_id(MakeTransportId(connection_id_, ++next_id_));
  pending_transactions_.push_back(
      new CreateViewTreeNodeTransaction(node_id, this));
  Sync();
  return node_id;
}

void ViewManagerSynchronizer::DestroyViewTreeNode(Id node_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new DestroyViewTreeNodeTransaction(node_id, this));
  Sync();
}

Id ViewManagerSynchronizer::CreateView() {
  DCHECK(connected_);
  const Id view_id(MakeTransportId(connection_id_, ++next_id_));
  pending_transactions_.push_back(new CreateViewTransaction(view_id, this));
  Sync();
  return view_id;
}

void ViewManagerSynchronizer::DestroyView(Id view_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(new DestroyViewTransaction(view_id, this));
  Sync();
}

void ViewManagerSynchronizer::AddChild(Id child_id,
                                       Id parent_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new HierarchyTransaction(HierarchyTransaction::TYPE_ADD,
                               child_id,
                               parent_id,
                               this));
  Sync();
}

void ViewManagerSynchronizer::RemoveChild(Id child_id, Id parent_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new HierarchyTransaction(HierarchyTransaction::TYPE_REMOVE,
                               child_id,
                               parent_id,
                               this));
  Sync();
}

bool ViewManagerSynchronizer::OwnsNode(Id id) const {
  return HiWord(id) == connection_id_;
}

bool ViewManagerSynchronizer::OwnsView(Id id) const {
  return HiWord(id) == connection_id_;
}

void ViewManagerSynchronizer::SetActiveView(Id node_id, Id view_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new SetActiveViewTransaction(node_id, view_id, this));
  Sync();
}

void ViewManagerSynchronizer::SetBounds(Id node_id, const gfx::Rect& bounds) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new SetBoundsTransaction(node_id, bounds, this));
  Sync();
}

void ViewManagerSynchronizer::SetViewContents(Id view_id,
                                              const SkBitmap& contents) {
  DCHECK(connected_);
  pending_transactions_.push_back(
      new SetViewContentsTransaction(view_id, contents, this));
  Sync();
}

void ViewManagerSynchronizer::Embed(const String& url, Id node_id) {
  DCHECK(connected_);
  pending_transactions_.push_back(new EmbedTransaction(url, node_id, this));
  Sync();
}

void ViewManagerSynchronizer::AddNode(ViewTreeNode* node) {
  DCHECK(nodes_.find(node->id()) == nodes_.end());
  nodes_[node->id()] = node;
}

void ViewManagerSynchronizer::RemoveNode(Id node_id) {
  IdToNodeMap::iterator it = nodes_.find(node_id);
  if (it != nodes_.end())
    nodes_.erase(it);
}

void ViewManagerSynchronizer::AddView(View* view) {
  DCHECK(views_.find(view->id()) == views_.end());
  views_[view->id()] = view;
}

void ViewManagerSynchronizer::RemoveView(Id view_id) {
  IdToViewMap::iterator it = views_.find(view_id);
  if (it != views_.end())
    views_.erase(it);
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerSynchronizer, ViewManager implementation:

const std::vector<ViewTreeNode*>& ViewManagerSynchronizer::GetRoots() const {
  return roots_;
}

ViewTreeNode* ViewManagerSynchronizer::GetNodeById(Id id) {
  IdToNodeMap::const_iterator it = nodes_.find(id);
  return it != nodes_.end() ? it->second : NULL;
}

View* ViewManagerSynchronizer::GetViewById(Id id) {
  IdToViewMap::const_iterator it = views_.find(id);
  return it != views_.end() ? it->second : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerSynchronizer, InterfaceImpl overrides:

void ViewManagerSynchronizer::OnConnectionEstablished() {
  service_ = client();
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerSynchronizer, IViewManagerClient implementation:

void ViewManagerSynchronizer::OnViewManagerConnectionEstablished(
    ConnectionSpecificId connection_id,
    Id next_server_change_id,
    mojo::Array<INodePtr> nodes) {
  connected_ = true;
  connection_id_ = connection_id;
  next_server_change_id_ = next_server_change_id;

  DCHECK(pending_transactions_.empty());
  AddRoot(BuildNodeTree(this, nodes));
}

void ViewManagerSynchronizer::OnRootsAdded(Array<INodePtr> nodes) {
  AddRoot(BuildNodeTree(this, nodes));
}

void ViewManagerSynchronizer::OnServerChangeIdAdvanced(
    Id next_server_change_id) {
  next_server_change_id_ = next_server_change_id;
}

void ViewManagerSynchronizer::OnNodeBoundsChanged(Id node_id,
                                                  RectPtr old_bounds,
                                                  RectPtr new_bounds) {
  ViewTreeNode* node = GetNodeById(node_id);
  ViewTreeNodePrivate(node).LocalSetBounds(old_bounds.To<gfx::Rect>(),
                                           new_bounds.To<gfx::Rect>());
}

void ViewManagerSynchronizer::OnNodeHierarchyChanged(
    Id node_id,
    Id new_parent_id,
    Id old_parent_id,
    Id server_change_id,
    mojo::Array<INodePtr> nodes) {
  // TODO: deal with |nodes|.
  next_server_change_id_ = server_change_id + 1;

  BuildNodeTree(this, nodes);

  ViewTreeNode* new_parent = GetNodeById(new_parent_id);
  ViewTreeNode* old_parent = GetNodeById(old_parent_id);
  ViewTreeNode* node = GetNodeById(node_id);
  if (new_parent)
    ViewTreeNodePrivate(new_parent).LocalAddChild(node);
  else
    ViewTreeNodePrivate(old_parent).LocalRemoveChild(node);
}

void ViewManagerSynchronizer::OnNodeDeleted(Id node_id, Id server_change_id) {
  next_server_change_id_ = server_change_id + 1;

  ViewTreeNode* node = GetNodeById(node_id);
  if (node)
    ViewTreeNodePrivate(node).LocalDestroy();
}

void ViewManagerSynchronizer::OnNodeViewReplaced(Id node_id,
                                                 Id new_view_id,
                                                 Id old_view_id) {
  ViewTreeNode* node = GetNodeById(node_id);
  View* new_view = GetViewById(new_view_id);
  if (!new_view && new_view_id != 0) {
    // This client wasn't aware of this View until now.
    new_view = ViewPrivate::LocalCreate();
    ViewPrivate private_view(new_view);
    private_view.set_view_manager(this);
    private_view.set_id(new_view_id);
    private_view.set_node(node);
    AddView(new_view);
  }
  View* old_view = GetViewById(old_view_id);
  DCHECK_EQ(old_view, node->active_view());
  ViewTreeNodePrivate(node).LocalSetActiveView(new_view);
}

void ViewManagerSynchronizer::OnViewDeleted(Id view_id) {
  View* view = GetViewById(view_id);
  if (view)
    ViewPrivate(view).LocalDestroy();
}

void ViewManagerSynchronizer::OnViewInputEvent(
    Id view_id,
    EventPtr event,
    const Callback<void()>& ack_callback) {
  View* view = GetViewById(view_id);
  if (view) {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(view).observers(),
                      OnViewInputEvent(view, event));
  }
  ack_callback.Run();
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

void ViewManagerSynchronizer::AddRoot(ViewTreeNode* root) {
  // A new root must not already exist as a root or be contained by an existing
  // hierarchy visible to this view manager.
  std::vector<ViewTreeNode*>::const_iterator it = roots_.begin();
  for (; it != roots_.end(); ++it) {
    if (*it == root || (*it)->Contains(root))
      return;
  }
  roots_.push_back(root);
  root->AddObserver(new RootObserver(root));
  delegate_->OnRootAdded(this, root);
}

void ViewManagerSynchronizer::RemoveRoot(ViewTreeNode* root) {
  std::vector<ViewTreeNode*>::iterator it =
      std::find(roots_.begin(), roots_.end(), root);
  if (it != roots_.end()) {
    roots_.erase(it);
    delegate_->OnRootRemoved(this, root);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ViewManager, public:

// static
void ViewManager::Create(Application* application,
                         ViewManagerDelegate* delegate) {
  application->AddService<ViewManagerSynchronizer>(delegate);
}

}  // namespace view_manager
}  // namespace mojo
