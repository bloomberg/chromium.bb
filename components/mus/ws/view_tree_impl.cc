// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/view_tree_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/default_access_policy.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/server_view.h"
#include "components/mus/ws/view_tree_host_impl.h"
#include "components/mus/ws/window_manager_access_policy.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/ime/ime_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "ui/platform_window/text_input_state.h"

using mojo::Array;
using mojo::Callback;
using mojo::InterfaceRequest;
using mojo::OrderDirection;
using mojo::Rect;
using mojo::ServiceProvider;
using mojo::ServiceProviderPtr;
using mojo::String;
using mojo::ViewDataPtr;

namespace mus {

ViewTreeImpl::ViewTreeImpl(ConnectionManager* connection_manager,
                           ConnectionSpecificId creator_id,
                           const ViewId& root_id,
                           uint32_t policy_bitmask)
    : connection_manager_(connection_manager),
      id_(connection_manager_->GetAndAdvanceNextConnectionId()),
      creator_id_(creator_id),
      client_(nullptr),
      is_embed_root_(false) {
  ServerView* view = GetView(root_id);
  CHECK(view);
  root_.reset(new ViewId(root_id));
  if (view->GetRoot() == view) {
    access_policy_.reset(new WindowManagerAccessPolicy(id_, this));
    is_embed_root_ = true;
  } else {
    access_policy_.reset(new DefaultAccessPolicy(id_, this));
    is_embed_root_ = (policy_bitmask & ViewTree::ACCESS_POLICY_EMBED_ROOT) != 0;
  }
}

ViewTreeImpl::~ViewTreeImpl() {
  DestroyViews();
}

void ViewTreeImpl::Init(mojo::ViewTreeClient* client, mojo::ViewTreePtr tree) {
  DCHECK(!client_);
  client_ = client;
  std::vector<const ServerView*> to_send;
  if (root_.get())
    GetUnknownViewsFrom(GetView(*root_), &to_send);

  // TODO(beng): verify that host can actually be nullptr here.
  ViewTreeHostImpl* host = GetHost();
  const ServerView* focused_view = host ? host->GetFocusedView() : nullptr;
  if (focused_view)
    focused_view = access_policy_->GetViewForFocusChange(focused_view);
  const Id focused_view_transport_id(
      ViewIdToTransportId(focused_view ? focused_view->id() : ViewId()));

  client->OnEmbed(id_, ViewToViewData(to_send.front()), tree.Pass(),
                  focused_view_transport_id,
                  is_embed_root_ ? ViewTree::ACCESS_POLICY_EMBED_ROOT
                                 : ViewTree::ACCESS_POLICY_DEFAULT);
}

const ServerView* ViewTreeImpl::GetView(const ViewId& id) const {
  if (id_ == id.connection_id) {
    ViewMap::const_iterator i = view_map_.find(id.view_id);
    return i == view_map_.end() ? NULL : i->second;
  }
  return connection_manager_->GetView(id);
}

bool ViewTreeImpl::IsRoot(const ViewId& id) const {
  return root_.get() && *root_ == id;
}

ViewTreeHostImpl* ViewTreeImpl::GetHost() {
  return root_.get()
             ? connection_manager_->GetViewTreeHostByView(GetView(*root_))
             : nullptr;
}

void ViewTreeImpl::OnWillDestroyViewTreeImpl(ViewTreeImpl* connection) {
  if (creator_id_ == connection->id())
    creator_id_ = kInvalidConnectionId;
  const ServerView* connection_root =
      connection->root_ ? connection->GetView(*connection->root_) : nullptr;
  if (connection_root &&
      ((connection_root->id().connection_id == id_ &&
        view_map_.count(connection_root->id().view_id) > 0) ||
       (is_embed_root_ && IsViewKnown(connection_root)))) {
    client()->OnEmbeddedAppDisconnected(
        ViewIdToTransportId(*connection->root_));
  }
  if (root_.get() && root_->connection_id == connection->id())
    root_.reset();
}

mojo::ErrorCode ViewTreeImpl::CreateView(const ViewId& view_id) {
  if (view_id.connection_id != id_)
    return mojo::ERROR_CODE_ILLEGAL_ARGUMENT;
  if (view_map_.find(view_id.view_id) != view_map_.end())
    return mojo::ERROR_CODE_VALUE_IN_USE;
  view_map_[view_id.view_id] = connection_manager_->CreateServerView(view_id);
  known_views_.insert(ViewIdToTransportId(view_id));
  return mojo::ERROR_CODE_NONE;
}

bool ViewTreeImpl::AddView(const ViewId& parent_id, const ViewId& child_id) {
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

std::vector<const ServerView*> ViewTreeImpl::GetViewTree(
    const ViewId& view_id) const {
  const ServerView* view = GetView(view_id);
  std::vector<const ServerView*> views;
  if (view)
    GetViewTreeImpl(view, &views);
  return views;
}

bool ViewTreeImpl::SetViewVisibility(const ViewId& view_id, bool visible) {
  ServerView* view = GetView(view_id);
  if (!view || view->visible() == visible ||
      !access_policy_->CanChangeViewVisibility(view)) {
    return false;
  }
  ConnectionManager::ScopedChange change(this, connection_manager_, false);
  view->SetVisible(visible);
  return true;
}

bool ViewTreeImpl::Embed(const ViewId& view_id,
                         mojo::ViewTreeClientPtr client,
                         uint32_t policy_bitmask,
                         ConnectionSpecificId* connection_id) {
  *connection_id = kInvalidConnectionId;
  if (!client.get() || !CanEmbed(view_id, policy_bitmask))
    return false;
  PrepareForEmbed(view_id);
  ViewTreeImpl* new_connection = connection_manager_->EmbedAtView(
      id_, view_id, policy_bitmask, client.Pass());
  if (is_embed_root_)
    *connection_id = new_connection->id();
  return true;
}

void ViewTreeImpl::Embed(const ViewId& view_id, mojo::URLRequestPtr request) {
  if (!CanEmbed(view_id, ViewTree::ACCESS_POLICY_DEFAULT))
    return;
  PrepareForEmbed(view_id);
  connection_manager_->EmbedAtView(
      id_, view_id, mojo::ViewTree::ACCESS_POLICY_DEFAULT, request.Pass());
}

void ViewTreeImpl::ProcessViewBoundsChanged(const ServerView* view,
                                            const gfx::Rect& old_bounds,
                                            const gfx::Rect& new_bounds,
                                            bool originated_change) {
  if (originated_change || !IsViewKnown(view))
    return;
  client()->OnWindowBoundsChanged(ViewIdToTransportId(view->id()),
                                  Rect::From(old_bounds),
                                  Rect::From(new_bounds));
}

void ViewTreeImpl::ProcessClientAreaChanged(const ServerView* window,
                                            const gfx::Rect& old_client_area,
                                            const gfx::Rect& new_client_area,
                                            bool originated_change) {
  if (originated_change || !IsViewKnown(window))
    return;
  client()->OnClientAreaChanged(ViewIdToTransportId(window->id()),
                                Rect::From(old_client_area),
                                Rect::From(new_client_area));
}

void ViewTreeImpl::ProcessViewportMetricsChanged(
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics,
    bool originated_change) {
  client()->OnWindowViewportMetricsChanged(old_metrics.Clone(),
                                           new_metrics.Clone());
}

void ViewTreeImpl::ProcessWillChangeViewHierarchy(const ServerView* view,
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

void ViewTreeImpl::ProcessViewPropertyChanged(
    const ServerView* view,
    const std::string& name,
    const std::vector<uint8_t>* new_data,
    bool originated_change) {
  if (originated_change)
    return;

  Array<uint8_t> data;
  if (new_data)
    data = Array<uint8_t>::From(*new_data);

  client()->OnWindowSharedPropertyChanged(ViewIdToTransportId(view->id()),
                                          String(name), data.Pass());
}

void ViewTreeImpl::ProcessViewHierarchyChanged(const ServerView* view,
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

  if (!access_policy_->ShouldNotifyOnHierarchyChange(view, &new_parent,
                                                     &old_parent)) {
    return;
  }
  // Inform the client of any new views and update the set of views we know
  // about.
  std::vector<const ServerView*> to_send;
  if (!IsViewKnown(view))
    GetUnknownViewsFrom(view, &to_send);
  const ViewId new_parent_id(new_parent ? new_parent->id() : ViewId());
  const ViewId old_parent_id(old_parent ? old_parent->id() : ViewId());
  client()->OnWindowHierarchyChanged(
      ViewIdToTransportId(view->id()), ViewIdToTransportId(new_parent_id),
      ViewIdToTransportId(old_parent_id), ViewsToViewDatas(to_send));
  connection_manager_->OnConnectionMessagedClient(id_);
}

void ViewTreeImpl::ProcessViewReorder(const ServerView* view,
                                      const ServerView* relative_view,
                                      OrderDirection direction,
                                      bool originated_change) {
  if (originated_change || !IsViewKnown(view) || !IsViewKnown(relative_view))
    return;

  client()->OnWindowReordered(ViewIdToTransportId(view->id()),
                              ViewIdToTransportId(relative_view->id()),
                              direction);
}

void ViewTreeImpl::ProcessViewDeleted(const ViewId& view,
                                      bool originated_change) {
  if (view.connection_id == id_)
    view_map_.erase(view.view_id);

  const bool in_known = known_views_.erase(ViewIdToTransportId(view)) > 0;

  if (IsRoot(view))
    root_.reset();

  if (originated_change)
    return;

  if (in_known) {
    client()->OnWindowDeleted(ViewIdToTransportId(view));
    connection_manager_->OnConnectionMessagedClient(id_);
  }
}

void ViewTreeImpl::ProcessWillChangeViewVisibility(const ServerView* view,
                                                   bool originated_change) {
  if (originated_change)
    return;

  if (IsViewKnown(view)) {
    client()->OnWindowVisibilityChanged(ViewIdToTransportId(view->id()),
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

void ViewTreeImpl::ProcessFocusChanged(const ServerView* old_focused_view,
                                       const ServerView* new_focused_view) {
  const ServerView* view =
      new_focused_view ? access_policy_->GetViewForFocusChange(new_focused_view)
                       : nullptr;
  client()->OnWindowFocused(view ? ViewIdToTransportId(view->id())
                                 : ViewIdToTransportId(ViewId()));
}

bool ViewTreeImpl::IsViewKnown(const ServerView* view) const {
  return known_views_.count(ViewIdToTransportId(view->id())) > 0;
}

bool ViewTreeImpl::CanReorderView(const ServerView* view,
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

bool ViewTreeImpl::DeleteViewImpl(ViewTreeImpl* source, ServerView* view) {
  DCHECK(view);
  DCHECK_EQ(view->id().connection_id, id_);
  ConnectionManager::ScopedChange change(source, connection_manager_, true);
  delete view;
  return true;
}

void ViewTreeImpl::GetUnknownViewsFrom(const ServerView* view,
                                       std::vector<const ServerView*>* views) {
  if (IsViewKnown(view) || !access_policy_->CanGetViewTree(view))
    return;
  views->push_back(view);
  known_views_.insert(ViewIdToTransportId(view->id()));
  if (!access_policy_->CanDescendIntoViewForViewTree(view))
    return;
  std::vector<const ServerView*> children(view->GetChildren());
  for (size_t i = 0; i < children.size(); ++i)
    GetUnknownViewsFrom(children[i], views);
}

void ViewTreeImpl::RemoveFromKnown(const ServerView* view,
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

void ViewTreeImpl::RemoveRoot() {
  CHECK(root_.get());
  const ViewId root_id(*root_);
  root_.reset();
  // No need to do anything if we created the view.
  if (root_id.connection_id == id_)
    return;

  client()->OnUnembed();
  client()->OnWindowDeleted(ViewIdToTransportId(root_id));
  connection_manager_->OnConnectionMessagedClient(id_);

  // This connection no longer knows about the view. Unparent any views that
  // were parented to views in the root.
  std::vector<ServerView*> local_views;
  RemoveFromKnown(GetView(root_id), &local_views);
  for (size_t i = 0; i < local_views.size(); ++i)
    local_views[i]->parent()->Remove(local_views[i]);
}

Array<ViewDataPtr> ViewTreeImpl::ViewsToViewDatas(
    const std::vector<const ServerView*>& views) {
  Array<ViewDataPtr> array(views.size());
  for (size_t i = 0; i < views.size(); ++i)
    array[i] = ViewToViewData(views[i]).Pass();
  return array.Pass();
}

ViewDataPtr ViewTreeImpl::ViewToViewData(const ServerView* view) {
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

void ViewTreeImpl::GetViewTreeImpl(
    const ServerView* view,
    std::vector<const ServerView*>* views) const {
  DCHECK(view);

  if (!access_policy_->CanGetViewTree(view))
    return;

  views->push_back(view);

  if (!access_policy_->CanDescendIntoViewForViewTree(view))
    return;

  std::vector<const ServerView*> children(view->GetChildren());
  for (size_t i = 0; i < children.size(); ++i)
    GetViewTreeImpl(children[i], views);
}

void ViewTreeImpl::NotifyDrawnStateChanged(const ServerView* view,
                                           bool new_drawn_value) {
  // Even though we don't know about view, it may be an ancestor of our root, in
  // which case the change may effect our roots drawn state.
  if (!root_.get())
    return;

  const ServerView* root = GetView(*root_);
  DCHECK(root);
  if (view->Contains(root) && (new_drawn_value != root->IsDrawn())) {
    client()->OnWindowDrawnStateChanged(ViewIdToTransportId(root->id()),
                                        new_drawn_value);
  }
}

void ViewTreeImpl::DestroyViews() {
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

bool ViewTreeImpl::CanEmbed(const ViewId& view_id,
                            uint32_t policy_bitmask) const {
  const ServerView* view = GetView(view_id);
  return view && access_policy_->CanEmbed(view, policy_bitmask);
}

void ViewTreeImpl::PrepareForEmbed(const ViewId& view_id) {
  const ServerView* view = GetView(view_id);
  DCHECK(view);

  // Only allow a node to be the root for one connection.
  ViewTreeImpl* existing_owner =
      connection_manager_->GetConnectionWithRoot(view_id);

  ConnectionManager::ScopedChange change(this, connection_manager_, true);
  RemoveChildrenAsPartOfEmbed(view_id);
  if (existing_owner) {
    // Never message the originating connection.
    connection_manager_->OnConnectionMessagedClient(id_);
    existing_owner->RemoveRoot();
  }
}

void ViewTreeImpl::RemoveChildrenAsPartOfEmbed(const ViewId& view_id) {
  ServerView* view = GetView(view_id);
  CHECK(view);
  CHECK(view->id().connection_id == view_id.connection_id);
  std::vector<ServerView*> children = view->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    view->Remove(children[i]);
}

void ViewTreeImpl::CreateView(Id transport_view_id,
                              const Callback<void(mojo::ErrorCode)>& callback) {
  callback.Run(CreateView(ViewIdFromTransportId(transport_view_id)));
}

void ViewTreeImpl::DeleteView(Id transport_view_id,
                              const Callback<void(bool)>& callback) {
  ServerView* view = GetView(ViewIdFromTransportId(transport_view_id));
  bool success = false;
  if (view && access_policy_->CanDeleteView(view)) {
    ViewTreeImpl* connection =
        connection_manager_->GetConnection(view->id().connection_id);
    success = connection && connection->DeleteViewImpl(this, view);
  }
  callback.Run(success);
}

void ViewTreeImpl::AddView(Id parent_id,
                           Id child_id,
                           const Callback<void(bool)>& callback) {
  callback.Run(AddView(ViewIdFromTransportId(parent_id),
                       ViewIdFromTransportId(child_id)));
}

void ViewTreeImpl::RemoveViewFromParent(Id view_id,
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

void ViewTreeImpl::ReorderView(Id view_id,
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

void ViewTreeImpl::GetViewTree(
    Id view_id,
    const Callback<void(Array<ViewDataPtr>)>& callback) {
  std::vector<const ServerView*> views(
      GetViewTree(ViewIdFromTransportId(view_id)));
  callback.Run(ViewsToViewDatas(views));
}

void ViewTreeImpl::SetViewBounds(Id view_id,
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

void ViewTreeImpl::SetViewVisibility(Id transport_view_id,
                                     bool visible,
                                     const Callback<void(bool)>& callback) {
  callback.Run(
      SetViewVisibility(ViewIdFromTransportId(transport_view_id), visible));
}

void ViewTreeImpl::SetViewProperty(uint32_t view_id,
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

void ViewTreeImpl::RequestSurface(Id view_id,
                                  mojo::InterfaceRequest<mojo::Surface> surface,
                                  mojo::SurfaceClientPtr client) {
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  const bool success = view && access_policy_->CanSetWindowSurfaceId(view);
  if (!success)
    return;
  view->Bind(surface.Pass(), client.Pass());
}

void ViewTreeImpl::SetViewTextInputState(uint32_t view_id,
                                         mojo::TextInputStatePtr state) {
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  bool success = view && access_policy_->CanSetViewTextInputState(view);
  if (success)
    view->SetTextInputState(state.To<ui::TextInputState>());
}

void ViewTreeImpl::SetImeVisibility(Id transport_view_id,
                                    bool visible,
                                    mojo::TextInputStatePtr state) {
  ServerView* view = GetView(ViewIdFromTransportId(transport_view_id));
  bool success = view && access_policy_->CanSetViewTextInputState(view);
  if (success) {
    if (!state.is_null())
      view->SetTextInputState(state.To<ui::TextInputState>());

    ViewTreeHostImpl* host = GetHost();
    if (host)
      host->SetImeVisibility(view, visible);
  }
}

void ViewTreeImpl::SetClientArea(Id transport_window_id, mojo::RectPtr rect) {
  ServerView* window = GetView(ViewIdFromTransportId(transport_window_id));
  if (!window || !access_policy_->CanSetClientArea(window))
    return;

  if (rect.is_null())
    window->SetClientArea(gfx::Rect(window->bounds().size()));
  else
    window->SetClientArea(rect.To<gfx::Rect>());
}

void ViewTreeImpl::Embed(Id transport_view_id,
                         mojo::ViewTreeClientPtr client,
                         uint32_t policy_bitmask,
                         const EmbedCallback& callback) {
  ConnectionSpecificId connection_id = kInvalidConnectionId;
  const bool result = Embed(ViewIdFromTransportId(transport_view_id),
                            client.Pass(), policy_bitmask, &connection_id);
  callback.Run(result, connection_id);
}

void ViewTreeImpl::SetFocus(uint32_t view_id) {
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  // TODO(beng): consider shifting non-policy drawn check logic to VTH's
  //             FocusController.
  if (view && view->IsDrawn() && access_policy_->CanSetFocus(view)) {
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    ViewTreeHostImpl* host = GetHost();
    if (host)
      host->SetFocusedView(view);
  }
}

bool ViewTreeImpl::IsRootForAccessPolicy(const ViewId& id) const {
  return IsRoot(id);
}

bool ViewTreeImpl::IsViewKnownForAccessPolicy(const ServerView* view) const {
  return IsViewKnown(view);
}

bool ViewTreeImpl::IsViewRootOfAnotherConnectionForAccessPolicy(
    const ServerView* view) const {
  ViewTreeImpl* connection =
      connection_manager_->GetConnectionWithRoot(view->id());
  return connection && connection != this;
}

bool ViewTreeImpl::IsDescendantOfEmbedRoot(const ServerView* view) {
  return is_embed_root_ && root_ && GetView(*root_)->Contains(view);
}

}  // namespace mus
