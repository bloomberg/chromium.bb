// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_client_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/public/cpp/view_manager/lib/node_private.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/window_manager_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {

Id MakeTransportId(ConnectionSpecificId connection_id,
                   ConnectionSpecificId local_id) {
  return (connection_id << 16) | local_id;
}

// Helper called to construct a local node/view object from transport data.
Node* AddNodeToViewManager(ViewManagerClientImpl* client,
                           Node* parent,
                           Id node_id,
                           const gfx::Rect& bounds) {
  // We don't use the ctor that takes a ViewManager here, since it will call
  // back to the service and attempt to create a new node.
  Node* node = NodePrivate::LocalCreate();
  NodePrivate private_node(node);
  private_node.set_view_manager(client);
  private_node.set_id(node_id);
  client->AddNode(node);
  private_node.LocalSetBounds(gfx::Rect(), bounds);
  if (parent)
    NodePrivate(parent).LocalAddChild(node);
  return node;
}

Node* BuildNodeTree(ViewManagerClientImpl* client,
                    const Array<NodeDataPtr>& nodes,
                    Node* initial_parent) {
  std::vector<Node*> parents;
  Node* root = NULL;
  Node* last_node = NULL;
  if (initial_parent)
    parents.push_back(initial_parent);
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (last_node && nodes[i]->parent_id == last_node->id()) {
      parents.push_back(last_node);
    } else if (!parents.empty()) {
      while (parents.back()->id() != nodes[i]->parent_id)
        parents.pop_back();
    }
    Node* node = AddNodeToViewManager(
        client,
        !parents.empty() ? parents.back() : NULL,
        nodes[i]->node_id,
        nodes[i]->bounds.To<gfx::Rect>());
    if (!last_node)
      root = node;
    last_node = node;
  }
  return root;
}

// Responsible for removing a root from the ViewManager when that node is
// destroyed.
class RootObserver : public NodeObserver {
 public:
  explicit RootObserver(Node* root) : root_(root) {}
  virtual ~RootObserver() {}

 private:
  // Overridden from NodeObserver:
  virtual void OnNodeDestroyed(Node* node) OVERRIDE {
    DCHECK_EQ(node, root_);
    static_cast<ViewManagerClientImpl*>(
        NodePrivate(root_).view_manager())->RemoveRoot(root_);
    node->RemoveObserver(this);
    delete this;
  }

  Node* root_;

  DISALLOW_COPY_AND_ASSIGN(RootObserver);
};

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

ViewManagerClientImpl::ViewManagerClientImpl(ViewManagerDelegate* delegate)
    : connected_(false),
      connection_id_(0),
      next_id_(1),
      delegate_(delegate),
      window_manager_delegate_(NULL) {
}

ViewManagerClientImpl::~ViewManagerClientImpl() {
  std::vector<Node*> non_owned;
  while (!nodes_.empty()) {
    IdToNodeMap::iterator it = nodes_.begin();
    if (OwnsNode(it->second->id())) {
      it->second->Destroy();
    } else {
      non_owned.push_back(it->second);
      nodes_.erase(it);
    }
  }
  // Delete the non-owned nodes last. In the typical case these are roots. The
  // exception is the window manager, which may know aboutother random nodes
  // that it doesn't own.
  // NOTE: we manually delete as we're a friend.
  for (size_t i = 0; i < non_owned.size(); ++i)
    delete non_owned[i];
  delegate_->OnViewManagerDisconnected(this);
}

Id ViewManagerClientImpl::CreateNode() {
  DCHECK(connected_);
  const Id node_id = MakeTransportId(connection_id_, ++next_id_);
  service_->CreateNode(node_id, ActionCompletedCallbackWithErrorCode());
  return node_id;
}

void ViewManagerClientImpl::DestroyNode(Id node_id) {
  DCHECK(connected_);
  service_->DeleteNode(node_id, ActionCompletedCallback());
}

void ViewManagerClientImpl::AddChild(Id child_id, Id parent_id) {
  DCHECK(connected_);
  service_->AddNode(parent_id, child_id, ActionCompletedCallback());
}

void ViewManagerClientImpl::RemoveChild(Id child_id, Id parent_id) {
  DCHECK(connected_);
  service_->RemoveNodeFromParent(child_id, ActionCompletedCallback());
}

void ViewManagerClientImpl::Reorder(
    Id node_id,
    Id relative_node_id,
    OrderDirection direction) {
  DCHECK(connected_);
  service_->ReorderNode(node_id, relative_node_id, direction,
                        ActionCompletedCallback());
}

bool ViewManagerClientImpl::OwnsNode(Id id) const {
  return HiWord(id) == connection_id_;
}

void ViewManagerClientImpl::SetBounds(Id node_id, const gfx::Rect& bounds) {
  DCHECK(connected_);
  service_->SetNodeBounds(node_id, Rect::From(bounds),
                          ActionCompletedCallback());
}

void ViewManagerClientImpl::SetNodeContents(Id node_id,
                                            const SkBitmap& contents) {
  DCHECK(connected_);
  std::vector<unsigned char> data;
  gfx::PNGCodec::EncodeBGRASkBitmap(contents, false, &data);

  void* memory = NULL;
  ScopedSharedBufferHandle duped, shared_state_handle;
  bool result = CreateMapAndDupSharedBuffer(data.size(),
                                            &memory,
                                            &shared_state_handle,
                                            &duped);
  if (!result)
    return;

  memcpy(memory, &data[0], data.size());

  service_->SetNodeContents(node_id, duped.Pass(),
                            static_cast<uint32_t>(data.size()),
                            ActionCompletedCallback());
}

void ViewManagerClientImpl::SetFocus(Id node_id) {
  DCHECK(connected_);
  service_->SetFocus(node_id, ActionCompletedCallback());
}

void ViewManagerClientImpl::SetVisible(Id node_id, bool visible) {
  DCHECK(connected_);
  service_->SetNodeVisibility(node_id, visible, ActionCompletedCallback());
}

void ViewManagerClientImpl::Embed(const String& url, Id node_id) {
  ServiceProviderPtr sp;
  BindToProxy(new ServiceProviderImpl, &sp);
  Embed(url, node_id, sp.Pass());
}

void ViewManagerClientImpl::Embed(
    const String& url,
    Id node_id,
    ServiceProviderPtr service_provider) {
  DCHECK(connected_);
  service_->Embed(url, node_id, service_provider.Pass(),
                  ActionCompletedCallback());
}

void ViewManagerClientImpl::AddNode(Node* node) {
  DCHECK(nodes_.find(node->id()) == nodes_.end());
  nodes_[node->id()] = node;
}

void ViewManagerClientImpl::RemoveNode(Id node_id) {
  IdToNodeMap::iterator it = nodes_.find(node_id);
  if (it != nodes_.end())
    nodes_.erase(it);
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, ViewManager implementation:

void ViewManagerClientImpl::SetWindowManagerDelegate(
    WindowManagerDelegate* window_manager_delegate) {
  CHECK(NULL != GetNodeById(1));
  window_manager_delegate_ = window_manager_delegate;
}

void ViewManagerClientImpl::DispatchEvent(Node* target, EventPtr event) {
  CHECK(window_manager_delegate_);
  service_->DispatchOnNodeInputEvent(target->id(), event.Pass());
}

const std::string& ViewManagerClientImpl::GetEmbedderURL() const {
  return creator_url_;
}

const std::vector<Node*>& ViewManagerClientImpl::GetRoots() const {
  return roots_;
}

Node* ViewManagerClientImpl::GetNodeById(Id id) {
  IdToNodeMap::const_iterator it = nodes_.find(id);
  return it != nodes_.end() ? it->second : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, InterfaceImpl overrides:

void ViewManagerClientImpl::OnConnectionEstablished() {
  service_ = client();
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, ViewManagerClient implementation:

void ViewManagerClientImpl::OnEmbed(
    ConnectionSpecificId connection_id,
    const String& creator_url,
    NodeDataPtr root_data,
    InterfaceRequest<ServiceProvider> service_provider) {
  if (!connected_) {
    connected_ = true;
    connection_id_ = connection_id;
    creator_url_ = TypeConverter<String, std::string>::ConvertFrom(creator_url);
  } else {
    DCHECK_EQ(connection_id_, connection_id);
    DCHECK_EQ(creator_url_, creator_url);
  }

  // A new root must not already exist as a root or be contained by an existing
  // hierarchy visible to this view manager.
  Node* root = AddNodeToViewManager(this, NULL, root_data->node_id,
                                    root_data->bounds.To<gfx::Rect>());
  roots_.push_back(root);
  root->AddObserver(new RootObserver(root));

  // BindToRequest() binds the lifetime of |exported_services| to the pipe.
  ServiceProviderImpl* exported_services = new ServiceProviderImpl;
  BindToRequest(exported_services, &service_provider);
  scoped_ptr<ServiceProvider> remote(
      exported_services->CreateRemoteServiceProvider());
  delegate_->OnEmbed(this, root, exported_services, remote.Pass());
}

void ViewManagerClientImpl::OnNodeBoundsChanged(Id node_id,
                                                RectPtr old_bounds,
                                                RectPtr new_bounds) {
  Node* node = GetNodeById(node_id);
  NodePrivate(node).LocalSetBounds(old_bounds.To<gfx::Rect>(),
                                   new_bounds.To<gfx::Rect>());
}

void ViewManagerClientImpl::OnNodeHierarchyChanged(
    Id node_id,
    Id new_parent_id,
    Id old_parent_id,
    mojo::Array<NodeDataPtr> nodes) {
  Node* initial_parent = nodes.size() ?
      GetNodeById(nodes[0]->parent_id) : NULL;

  BuildNodeTree(this, nodes, initial_parent);

  Node* new_parent = GetNodeById(new_parent_id);
  Node* old_parent = GetNodeById(old_parent_id);
  Node* node = GetNodeById(node_id);
  if (new_parent)
    NodePrivate(new_parent).LocalAddChild(node);
  else
    NodePrivate(old_parent).LocalRemoveChild(node);
}

void ViewManagerClientImpl::OnNodeReordered(Id node_id,
                                            Id relative_node_id,
                                            OrderDirection direction) {
  Node* node = GetNodeById(node_id);
  Node* relative_node = GetNodeById(relative_node_id);
  if (node && relative_node)
    NodePrivate(node).LocalReorder(relative_node, direction);
}

void ViewManagerClientImpl::OnNodeDeleted(Id node_id) {
  Node* node = GetNodeById(node_id);
  if (node)
    NodePrivate(node).LocalDestroy();
}

void ViewManagerClientImpl::OnNodeInputEvent(
    Id node_id,
    EventPtr event,
    const Callback<void()>& ack_callback) {
  Node* node = GetNodeById(node_id);
  if (node) {
    FOR_EACH_OBSERVER(NodeObserver,
                      *NodePrivate(node).observers(),
                      OnNodeInputEvent(node, event));
  }
  ack_callback.Run();
}

void ViewManagerClientImpl::OnFocusChanged(Id gained_focus_id,
                                           Id lost_focus_id) {
  Node* focused = GetNodeById(gained_focus_id);
  Node* blurred = GetNodeById(lost_focus_id);
  if (blurred) {
    FOR_EACH_OBSERVER(NodeObserver,
                      *NodePrivate(blurred).observers(),
                      OnNodeFocusChanged(focused, blurred));
  }
  if (focused) {
    FOR_EACH_OBSERVER(NodeObserver,
                      *NodePrivate(focused).observers(),
                      OnNodeFocusChanged(focused, blurred));
  }
}

void ViewManagerClientImpl::Embed(
    const String& url,
    InterfaceRequest<ServiceProvider> service_provider) {
  window_manager_delegate_->Embed(url, service_provider.Pass());
}

void ViewManagerClientImpl::DispatchOnNodeInputEvent(Id node_id,
                                                     EventPtr event) {
  if (window_manager_delegate_)
    window_manager_delegate_->DispatchEvent(GetNodeById(node_id), event.Pass());
}

////////////////////////////////////////////////////////////////////////////////
// ViewManagerClientImpl, private:

void ViewManagerClientImpl::RemoveRoot(Node* root) {
  std::vector<Node*>::iterator it =
      std::find(roots_.begin(), roots_.end(), root);
  if (it != roots_.end())
    roots_.erase(it);
}

void ViewManagerClientImpl::OnActionCompleted(bool success) {
  if (!change_acked_callback_.is_null())
    change_acked_callback_.Run();
}

void ViewManagerClientImpl::OnActionCompletedWithErrorCode(ErrorCode code) {
  OnActionCompleted(code == ERROR_CODE_NONE);
}

base::Callback<void(bool)> ViewManagerClientImpl::ActionCompletedCallback() {
  return base::Bind(&ViewManagerClientImpl::OnActionCompleted,
                    base::Unretained(this));
}

base::Callback<void(ErrorCode)>
    ViewManagerClientImpl::ActionCompletedCallbackWithErrorCode() {
  return base::Bind(&ViewManagerClientImpl::OnActionCompletedWithErrorCode,
                    base::Unretained(this));
}

}  // namespace mojo
