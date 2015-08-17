// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/view_manager_service_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "components/view_manager/connection_manager.h"
#include "components/view_manager/default_access_policy.h"
#include "components/view_manager/display_manager.h"
#include "components/view_manager/server_view.h"
#include "components/view_manager/window_manager_access_policy.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/ime/ime_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "ui/platform_window/text_input_state.h"

using mojo::Array;
using mojo::Callback;
using mojo::Id;
using mojo::InterfaceRequest;
using mojo::OrderDirection;
using mojo::Rect;
using mojo::ServiceProvider;
using mojo::ServiceProviderPtr;
using mojo::String;
using mojo::ViewDataPtr;

namespace view_manager {

// Contains information needed to complete an Embed(). See description of
// |pending_embeds_| for more details.
struct ViewManagerServiceImpl::PendingEmbed
    : public base::RefCounted<PendingEmbed> {
  PendingEmbed() : embed_root(nullptr) {}

  ViewManagerServiceImpl* embed_root;
  ViewId view_id;
  mojo::Callback<void(bool)> callback;

 private:
  friend class base::RefCounted<PendingEmbed>;

  ~PendingEmbed() {}
};

ViewManagerServiceImpl::ViewManagerServiceImpl(
    ConnectionManager* connection_manager,
    mojo::ConnectionSpecificId creator_id,
    const ViewId& root_id)
    : connection_manager_(connection_manager),
      id_(connection_manager_->GetAndAdvanceNextConnectionId()),
      creator_id_(creator_id),
      client_(nullptr),
      is_embed_root_(false) {
  ServerView* view = GetView(root_id);
  CHECK(view);
  root_.reset(new ViewId(root_id));
  if (view->GetRoot() == view)
    access_policy_.reset(new WindowManagerAccessPolicy(id_, this));
  else
    access_policy_.reset(new DefaultAccessPolicy(id_, this));
}

ViewManagerServiceImpl::~ViewManagerServiceImpl() {
  DestroyViews();
}

void ViewManagerServiceImpl::Init(mojo::ViewManagerClient* client,
                                  mojo::ViewManagerServicePtr service_ptr) {
  DCHECK(!client_);
  client_ = client;
  std::vector<const ServerView*> to_send;
  if (root_.get())
    GetUnknownViewsFrom(GetView(*root_), &to_send);

  const ServerView* focused_view = connection_manager_->GetFocusedView();
  if (focused_view)
    focused_view = access_policy_->GetViewForFocusChange(focused_view);
  const mojo::Id focused_view_transport_id(
      ViewIdToTransportId(focused_view ? focused_view->id() : ViewId()));

  client->OnEmbed(id_, ViewToViewData(to_send.front()), service_ptr.Pass(),
                  focused_view_transport_id);
}

const ServerView* ViewManagerServiceImpl::GetView(const ViewId& id) const {
  if (id_ == id.connection_id) {
    ViewMap::const_iterator i = view_map_.find(id.view_id);
    return i == view_map_.end() ? NULL : i->second;
  }
  return connection_manager_->GetView(id);
}

bool ViewManagerServiceImpl::IsRoot(const ViewId& id) const {
  return root_.get() && *root_ == id;
}

void ViewManagerServiceImpl::OnWillDestroyViewManagerServiceImpl(
    ViewManagerServiceImpl* connection) {
  InvalidatePendingEmbedForConnection(connection);
  if (creator_id_ == connection->id())
    creator_id_ = kInvalidConnectionId;
  if (connection->root_ && connection->root_->connection_id == id_ &&
      view_map_.count(connection->root_->view_id) > 0) {
    client()->OnEmbeddedAppDisconnected(
        ViewIdToTransportId(*connection->root_));
  }
  if (root_.get() && root_->connection_id == connection->id())
    root_.reset();
}

mojo::ErrorCode ViewManagerServiceImpl::CreateView(const ViewId& view_id) {
  if (view_id.connection_id != id_)
    return mojo::ERROR_CODE_ILLEGAL_ARGUMENT;
  if (view_map_.find(view_id.view_id) != view_map_.end())
    return mojo::ERROR_CODE_VALUE_IN_USE;
  view_map_[view_id.view_id] = connection_manager_->CreateServerView(view_id);
  known_views_.insert(ViewIdToTransportId(view_id));
  return mojo::ERROR_CODE_NONE;
}

bool ViewManagerServiceImpl::AddView(const ViewId& parent_id,
                                     const ViewId& child_id) {
  ServerView* parent = GetView(parent_id);
  ServerView* child = GetView(child_id);
  if (parent && child && child->parent() != parent &&
      !child->Contains(parent) && access_policy_->CanAddView(parent, child)) {
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    parent->Add(child);
    return true;
  }
  return false;
}

std::vector<const ServerView*> ViewManagerServiceImpl::GetViewTree(
    const ViewId& view_id) const {
  const ServerView* view = GetView(view_id);
  std::vector<const ServerView*> views;
  if (view)
    GetViewTreeImpl(view, &views);
  return views;
}

bool ViewManagerServiceImpl::SetViewVisibility(const ViewId& view_id,
                                               bool visible) {
  ServerView* view = GetView(view_id);
  if (!view || view->visible() == visible ||
      !access_policy_->CanChangeViewVisibility(view)) {
    return false;
  }
  ConnectionManager::ScopedChange change(this, connection_manager_, false);
  view->SetVisible(visible);
  return true;
}

void ViewManagerServiceImpl::EmbedAllowingReembed(
    const ViewId& view_id,
    mojo::URLRequestPtr request,
    const mojo::Callback<void(bool)>& callback) {
  if (!CanEmbed(view_id)) {
    callback.Run(false);
    return;
  }

  ViewManagerServiceImpl* embed_root = nullptr;

  ServerView* view = GetView(view_id);
  DCHECK(view);  // CanEmbed() returns true only if |view_id| is valid.
  if (view->id().connection_id == id_) {
    view->set_allows_reembed(true);

    // Only consult the embed root if the creator is doing the embed. If someone
    // other than the creator is doing the embed they were granted embed access.
    embed_root = connection_manager_->GetEmbedRoot(this);
  }

  if (!embed_root) {
    PrepareForEmbed(view_id);
    connection_manager_->EmbedAtView(id_, view_id, request.Pass());
    callback.Run(true);
    return;
  }

  // There is an embed root. We have to query it before completing the embed.
  scoped_refptr<PendingEmbed> pending_embed(new PendingEmbed);
  pending_embeds_.insert(pending_embed);
  pending_embed->embed_root = embed_root;
  pending_embed->view_id = view_id;
  pending_embed->callback = callback;
  pending_embeds_.insert(pending_embed);
  embed_root->client()->OnEmbedForDescendant(
      ViewIdToTransportId(view_id), request.Pass(),
      base::Bind(&ViewManagerServiceImpl::OnEmbedForDescendantDone,
                 base::Unretained(this), pending_embed));
}

bool ViewManagerServiceImpl::Embed(const ViewId& view_id,
                                   mojo::ViewManagerClientPtr client) {
  if (!client.get() || !CanEmbed(view_id))
    return false;
  PrepareForEmbed(view_id);
  ::ignore_result(
      connection_manager_->EmbedAtView(id_, view_id, client.Pass()));
  return true;
}

void ViewManagerServiceImpl::ProcessViewBoundsChanged(
    const ServerView* view,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    bool originated_change) {
  if (originated_change || !IsViewKnown(view))
    return;
  client()->OnViewBoundsChanged(ViewIdToTransportId(view->id()),
                                Rect::From(old_bounds),
                                Rect::From(new_bounds));
}

void ViewManagerServiceImpl::ProcessViewportMetricsChanged(
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics,
    bool originated_change) {
  client()->OnViewViewportMetricsChanged(old_metrics.Clone(),
                                         new_metrics.Clone());
}

void ViewManagerServiceImpl::ProcessWillChangeViewHierarchy(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent,
    bool originated_change) {
  if (originated_change)
    return;

  const bool old_drawn = view->IsDrawn();
  const bool new_drawn = view->visible() && new_parent && new_parent->IsDrawn();
  if (old_drawn == new_drawn)
    return;

  NotifyDrawnStateChanged(view, new_drawn);
}

void ViewManagerServiceImpl::ProcessViewPropertyChanged(
    const ServerView* view,
    const std::string& name,
    const std::vector<uint8_t>* new_data,
    bool originated_change) {
  if (originated_change)
    return;

  Array<uint8_t> data;
  if (new_data)
    data = Array<uint8_t>::From(*new_data);

  client()->OnViewSharedPropertyChanged(ViewIdToTransportId(view->id()),
                                        String(name), data.Pass());
}

void ViewManagerServiceImpl::ProcessViewHierarchyChanged(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent,
    bool originated_change) {
  if (originated_change && !IsViewKnown(view) && new_parent &&
      IsViewKnown(new_parent)) {
    std::vector<const ServerView*> unused;
    GetUnknownViewsFrom(view, &unused);
  }
  if (originated_change || connection_manager_->is_processing_delete_view() ||
      connection_manager_->DidConnectionMessageClient(id_)) {
    return;
  }

  if (!access_policy_->ShouldNotifyOnHierarchyChange(
          view, &new_parent, &old_parent)) {
    return;
  }
  // Inform the client of any new views and update the set of views we know
  // about.
  std::vector<const ServerView*> to_send;
  if (!IsViewKnown(view))
    GetUnknownViewsFrom(view, &to_send);
  const ViewId new_parent_id(new_parent ? new_parent->id() : ViewId());
  const ViewId old_parent_id(old_parent ? old_parent->id() : ViewId());
  client()->OnViewHierarchyChanged(ViewIdToTransportId(view->id()),
                                   ViewIdToTransportId(new_parent_id),
                                   ViewIdToTransportId(old_parent_id),
                                   ViewsToViewDatas(to_send));
  connection_manager_->OnConnectionMessagedClient(id_);
}

void ViewManagerServiceImpl::ProcessViewReorder(const ServerView* view,
                                                const ServerView* relative_view,
                                                OrderDirection direction,
                                                bool originated_change) {
  if (originated_change || !IsViewKnown(view) || !IsViewKnown(relative_view))
    return;

  client()->OnViewReordered(ViewIdToTransportId(view->id()),
                            ViewIdToTransportId(relative_view->id()),
                            direction);
}

void ViewManagerServiceImpl::ProcessViewDeleted(const ViewId& view,
                                                bool originated_change) {
  if (view.connection_id == id_)
    view_map_.erase(view.view_id);

  const bool in_known = known_views_.erase(ViewIdToTransportId(view)) > 0;

  if (IsRoot(view))
    root_.reset();

  InvalidatePendingEmbedForView(view);

  if (originated_change)
    return;

  if (in_known) {
    client()->OnViewDeleted(ViewIdToTransportId(view));
    connection_manager_->OnConnectionMessagedClient(id_);
  }
}

void ViewManagerServiceImpl::ProcessWillChangeViewVisibility(
    const ServerView* view,
    bool originated_change) {
  if (originated_change)
    return;

  if (IsViewKnown(view)) {
    client()->OnViewVisibilityChanged(ViewIdToTransportId(view->id()),
                                      !view->visible());
    return;
  }

  bool view_target_drawn_state;
  if (view->visible()) {
    // View is being hidden, won't be drawn.
    view_target_drawn_state = false;
  } else {
    // View is being shown. View will be drawn if its parent is drawn.
    view_target_drawn_state = view->parent() && view->parent()->IsDrawn();
  }

  NotifyDrawnStateChanged(view, view_target_drawn_state);
}

void ViewManagerServiceImpl::ProcessFocusChanged(
    const ServerView* old_focused_view,
    const ServerView* new_focused_view) {
  const ServerView* view =
      new_focused_view ? access_policy_->GetViewForFocusChange(new_focused_view)
                       : nullptr;
  client()->OnViewFocused(view ? ViewIdToTransportId(view->id())
                               : ViewIdToTransportId(ViewId()));
}

bool ViewManagerServiceImpl::IsViewKnown(const ServerView* view) const {
  return known_views_.count(ViewIdToTransportId(view->id())) > 0;
}

bool ViewManagerServiceImpl::CanReorderView(const ServerView* view,
                                            const ServerView* relative_view,
                                            OrderDirection direction) const {
  if (!view || !relative_view)
    return false;

  if (!view->parent() || view->parent() != relative_view->parent())
    return false;

  if (!access_policy_->CanReorderView(view, relative_view, direction))
    return false;

  std::vector<const ServerView*> children = view->parent()->GetChildren();
  const size_t child_i =
      std::find(children.begin(), children.end(), view) - children.begin();
  const size_t target_i =
      std::find(children.begin(), children.end(), relative_view) -
      children.begin();
  if ((direction == mojo::ORDER_DIRECTION_ABOVE && child_i == target_i + 1) ||
      (direction == mojo::ORDER_DIRECTION_BELOW && child_i + 1 == target_i)) {
    return false;
  }

  return true;
}

bool ViewManagerServiceImpl::DeleteViewImpl(ViewManagerServiceImpl* source,
                                            ServerView* view) {
  DCHECK(view);
  DCHECK_EQ(view->id().connection_id, id_);
  ConnectionManager::ScopedChange change(source, connection_manager_, true);
  delete view;
  return true;
}

void ViewManagerServiceImpl::GetUnknownViewsFrom(
    const ServerView* view,
    std::vector<const ServerView*>* views) {
  if (IsViewKnown(view) || !access_policy_->CanGetViewTree(view))
    return;
  views->push_back(view);
  known_views_.insert(ViewIdToTransportId(view->id()));
  if (!access_policy_->CanDescendIntoViewForViewTree(view))
    return;
  std::vector<const ServerView*> children(view->GetChildren());
  for (size_t i = 0 ; i < children.size(); ++i)
    GetUnknownViewsFrom(children[i], views);
}

void ViewManagerServiceImpl::RemoveFromKnown(
    const ServerView* view,
    std::vector<ServerView*>* local_views) {
  if (view->id().connection_id == id_) {
    if (local_views)
      local_views->push_back(GetView(view->id()));
    return;
  }
  known_views_.erase(ViewIdToTransportId(view->id()));
  std::vector<const ServerView*> children = view->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    RemoveFromKnown(children[i], local_views);
}

void ViewManagerServiceImpl::RemoveRoot() {
  CHECK(root_.get());
  const ViewId root_id(*root_);
  root_.reset();
  // No need to do anything if we created the view.
  if (root_id.connection_id == id_)
    return;

  client()->OnUnembed();
  client()->OnViewDeleted(ViewIdToTransportId(root_id));
  connection_manager_->OnConnectionMessagedClient(id_);

  // This connection no longer knows about the view. Unparent any views that
  // were parented to views in the root.
  std::vector<ServerView*> local_views;
  RemoveFromKnown(GetView(root_id), &local_views);
  for (size_t i = 0; i < local_views.size(); ++i)
    local_views[i]->parent()->Remove(local_views[i]);
}

Array<ViewDataPtr> ViewManagerServiceImpl::ViewsToViewDatas(
    const std::vector<const ServerView*>& views) {
  Array<ViewDataPtr> array(views.size());
  for (size_t i = 0; i < views.size(); ++i)
    array[i] = ViewToViewData(views[i]).Pass();
  return array.Pass();
}

ViewDataPtr ViewManagerServiceImpl::ViewToViewData(const ServerView* view) {
  DCHECK(IsViewKnown(view));
  const ServerView* parent = view->parent();
  // If the parent isn't known, it means the parent is not visible to us (not
  // in roots), and should not be sent over.
  if (parent && !IsViewKnown(parent))
    parent = NULL;
  ViewDataPtr view_data(mojo::ViewData::New());
  view_data->parent_id = ViewIdToTransportId(parent ? parent->id() : ViewId());
  view_data->view_id = ViewIdToTransportId(view->id());
  view_data->bounds = Rect::From(view->bounds());
  view_data->properties =
      mojo::Map<String, Array<uint8_t>>::From(view->properties());
  view_data->visible = view->visible();
  view_data->drawn = view->IsDrawn();
  view_data->viewport_metrics =
      connection_manager_->GetViewportMetricsForView(view);
  return view_data.Pass();
}

void ViewManagerServiceImpl::GetViewTreeImpl(
    const ServerView* view,
    std::vector<const ServerView*>* views) const {
  DCHECK(view);

  if (!access_policy_->CanGetViewTree(view))
    return;

  views->push_back(view);

  if (!access_policy_->CanDescendIntoViewForViewTree(view))
    return;

  std::vector<const ServerView*> children(view->GetChildren());
  for (size_t i = 0 ; i < children.size(); ++i)
    GetViewTreeImpl(children[i], views);
}

void ViewManagerServiceImpl::NotifyDrawnStateChanged(const ServerView* view,
                                                     bool new_drawn_value) {
  // Even though we don't know about view, it may be an ancestor of our root, in
  // which case the change may effect our roots drawn state.
  if (!root_.get())
    return;

  const ServerView* root = GetView(*root_);
  DCHECK(root);
  if (view->Contains(root) && (new_drawn_value != root->IsDrawn())) {
    client()->OnViewDrawnStateChanged(ViewIdToTransportId(root->id()),
                                      new_drawn_value);
  }
}

void ViewManagerServiceImpl::DestroyViews() {
  if (!view_map_.empty()) {
    ConnectionManager::ScopedChange change(this, connection_manager_, true);
    // If we get here from the destructor we're not going to get
    // ProcessViewDeleted(). Copy the map and delete from the copy so that we
    // don't have to worry about whether |view_map_| changes or not.
    ViewMap view_map_copy;
    view_map_.swap(view_map_copy);
    STLDeleteValues(&view_map_copy);
  }
}

bool ViewManagerServiceImpl::CanEmbed(const ViewId& view_id) const {
  const ServerView* view = GetView(view_id);
  return view && access_policy_->CanEmbed(view);
}

void ViewManagerServiceImpl::PrepareForEmbed(const ViewId& view_id) {
  const ServerView* view = GetView(view_id);
  DCHECK(view && access_policy_->CanEmbed(view));

  // Only allow a node to be the root for one connection.
  ViewManagerServiceImpl* existing_owner =
      connection_manager_->GetConnectionWithRoot(view_id);

  ConnectionManager::ScopedChange change(this, connection_manager_, true);
  RemoveChildrenAsPartOfEmbed(view_id);
  if (existing_owner) {
    // Never message the originating connection.
    connection_manager_->OnConnectionMessagedClient(id_);
    existing_owner->RemoveRoot();
  }
}

void ViewManagerServiceImpl::RemoveChildrenAsPartOfEmbed(
    const ViewId& view_id) {
  ServerView* view = GetView(view_id);
  CHECK(view);
  CHECK(view->id().connection_id == view_id.connection_id);
  std::vector<ServerView*> children = view->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    view->Remove(children[i]);
}

void ViewManagerServiceImpl::OnEmbedForDescendantDone(
    scoped_refptr<PendingEmbed> pending_embed,
    mojo::ViewManagerClientPtr client) {
  if (!pending_embeds_.count(pending_embed.get()))
    return;

  const bool allow_embed = client.get() && CanEmbed(pending_embed->view_id);
  if (allow_embed) {
    PrepareForEmbed(pending_embed->view_id);
    connection_manager_->EmbedAtView(id_, pending_embed->view_id,
                                     client.Pass());
  }
  RemovePendingEmbedAndNotifyCallback(pending_embed.get(), allow_embed);
}

void ViewManagerServiceImpl::InvalidatePendingEmbedForConnection(
    ViewManagerServiceImpl* connection) {
  if (pending_embeds_.empty())
    return;
  std::set<scoped_refptr<PendingEmbed>> copy(pending_embeds_);
  for (auto& embed : copy) {
    if (embed->embed_root == connection)
      RemovePendingEmbedAndNotifyCallback(embed, false);
  }
}

void ViewManagerServiceImpl::InvalidatePendingEmbedForView(
    const ViewId& view_id) {
  if (pending_embeds_.empty())
    return;
  std::set<scoped_refptr<PendingEmbed>> copy(pending_embeds_);
  for (auto& embed : copy) {
    if (embed->view_id == view_id)
      RemovePendingEmbedAndNotifyCallback(embed, false);
  }
}

void ViewManagerServiceImpl::RemovePendingEmbedAndNotifyCallback(
    scoped_refptr<PendingEmbed> embed,
    bool success) {
  pending_embeds_.erase(embed);
  embed->callback.Run(success);
}

void ViewManagerServiceImpl::CreateView(
    Id transport_view_id,
    const Callback<void(mojo::ErrorCode)>& callback) {
  callback.Run(CreateView(ViewIdFromTransportId(transport_view_id)));
}

void ViewManagerServiceImpl::DeleteView(
    Id transport_view_id,
    const Callback<void(bool)>& callback) {
  ServerView* view = GetView(ViewIdFromTransportId(transport_view_id));
  bool success = false;
  if (view && access_policy_->CanDeleteView(view)) {
    ViewManagerServiceImpl* connection =
        connection_manager_->GetConnection(view->id().connection_id);
    success = connection && connection->DeleteViewImpl(this, view);
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::AddView(
    Id parent_id,
    Id child_id,
    const Callback<void(bool)>& callback) {
  callback.Run(AddView(ViewIdFromTransportId(parent_id),
                       ViewIdFromTransportId(child_id)));
}

void ViewManagerServiceImpl::RemoveViewFromParent(
    Id view_id,
    const Callback<void(bool)>& callback) {
  bool success = false;
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  if (view && view->parent() && access_policy_->CanRemoveViewFromParent(view)) {
    success = true;
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    view->parent()->Remove(view);
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::ReorderView(Id view_id,
                                         Id relative_view_id,
                                         OrderDirection direction,
                                         const Callback<void(bool)>& callback) {
  bool success = false;
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  ServerView* relative_view = GetView(ViewIdFromTransportId(relative_view_id));
  if (CanReorderView(view, relative_view, direction)) {
    success = true;
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    view->parent()->Reorder(view, relative_view, direction);
    connection_manager_->ProcessViewReorder(view, relative_view, direction);
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::GetViewTree(
    Id view_id,
    const Callback<void(Array<ViewDataPtr>)>& callback) {
  std::vector<const ServerView*> views(
      GetViewTree(ViewIdFromTransportId(view_id)));
  callback.Run(ViewsToViewDatas(views));
}

void ViewManagerServiceImpl::SetViewSurfaceId(
    Id view_id,
    mojo::SurfaceIdPtr surface_id,
    const Callback<void(bool)>& callback) {
  // TODO(sky): add coverage of not being able to set for random node.
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  if (!view || !access_policy_->CanSetViewSurfaceId(view)) {
    callback.Run(false);
    return;
  }
  view->SetSurfaceId(surface_id.To<cc::SurfaceId>());
  callback.Run(true);
}

void ViewManagerServiceImpl::SetViewBounds(
    Id view_id,
    mojo::RectPtr bounds,
    const Callback<void(bool)>& callback) {
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  const bool success = view && access_policy_->CanSetViewBounds(view);
  if (success) {
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    view->SetBounds(bounds.To<gfx::Rect>());
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::SetViewVisibility(
    Id transport_view_id,
    bool visible,
    const Callback<void(bool)>& callback) {
  callback.Run(
      SetViewVisibility(ViewIdFromTransportId(transport_view_id), visible));
}

void ViewManagerServiceImpl::SetViewProperty(
    uint32_t view_id,
    const mojo::String& name,
    mojo::Array<uint8_t> value,
    const mojo::Callback<void(bool)>& callback) {
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  const bool success = view && access_policy_->CanSetViewProperties(view);
  if (success) {
    ConnectionManager::ScopedChange change(this, connection_manager_, false);

    if (value.is_null()) {
      view->SetProperty(name, nullptr);
    } else {
      std::vector<uint8_t> data = value.To<std::vector<uint8_t>>();
      view->SetProperty(name, &data);
    }
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::SetViewTextInputState(
    uint32_t view_id,
    mojo::TextInputStatePtr state) {
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  bool success = view && access_policy_->CanSetViewTextInputState(view);
  if (success)
    view->SetTextInputState(state.To<ui::TextInputState>());
}

void ViewManagerServiceImpl::SetImeVisibility(uint32_t view_id,
                                              bool visible,
                                              mojo::TextInputStatePtr state) {
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  bool success = view && access_policy_->CanSetViewTextInputState(view);
  if (success) {
    if (!state.is_null())
      view->SetTextInputState(state.To<ui::TextInputState>());
    connection_manager_->SetImeVisibility(view, visible);
  }
}

void ViewManagerServiceImpl::SetEmbedRoot() {
  is_embed_root_ = true;
}

void ViewManagerServiceImpl::Embed(mojo::Id transport_view_id,
                                   mojo::ViewManagerClientPtr client,
                                   const mojo::Callback<void(bool)>& callback) {
  callback.Run(Embed(ViewIdFromTransportId(transport_view_id), client.Pass()));
}

void ViewManagerServiceImpl::EmbedAllowingReembed(
    mojo::Id transport_view_id,
    mojo::URLRequestPtr request,
    const mojo::Callback<void(bool)>& callback) {
  EmbedAllowingReembed(ViewIdFromTransportId(transport_view_id), request.Pass(),
                       callback);
}

void ViewManagerServiceImpl::SetFocus(uint32_t view_id,
                                      const SetFocusCallback& callback) {
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  bool success = view && view->IsDrawn() && access_policy_->CanSetFocus(view);
  if (success) {
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    connection_manager_->SetFocusedView(view);
  }
  callback.Run(success);
}

bool ViewManagerServiceImpl::IsRootForAccessPolicy(const ViewId& id) const {
  return IsRoot(id);
}

bool ViewManagerServiceImpl::IsViewKnownForAccessPolicy(
    const ServerView* view) const {
  return IsViewKnown(view);
}

bool ViewManagerServiceImpl::IsViewRootOfAnotherConnectionForAccessPolicy(
    const ServerView* view) const {
  ViewManagerServiceImpl* connection =
      connection_manager_->GetConnectionWithRoot(view->id());
  return connection && connection != this;
}

bool ViewManagerServiceImpl::IsEmbedRootForAccessPolicy() {
  return is_embed_root_;
}

}  // namespace view_manager
