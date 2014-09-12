// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_service_impl.h"

#include "base/bind.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"
#include "mojo/services/view_manager/connection_manager.h"
#include "mojo/services/view_manager/default_access_policy.h"
#include "mojo/services/view_manager/server_view.h"
#include "mojo/services/view_manager/window_manager_access_policy.h"

namespace mojo {
namespace service {

ViewManagerServiceImpl::ViewManagerServiceImpl(
    ConnectionManager* connection_manager,
    ConnectionSpecificId creator_id,
    const std::string& creator_url,
    const std::string& url,
    const ViewId& root_id,
    InterfaceRequest<ServiceProvider> service_provider)
    : connection_manager_(connection_manager),
      id_(connection_manager_->GetAndAdvanceNextConnectionId()),
      url_(url),
      creator_id_(creator_id),
      creator_url_(creator_url),
      delete_on_connection_error_(false),
      service_provider_(service_provider.Pass()) {
  CHECK(GetView(root_id));
  roots_.insert(ViewIdToTransportId(root_id));
  if (root_id == RootViewId())
    access_policy_.reset(new WindowManagerAccessPolicy(id_, this));
  else
    access_policy_.reset(new DefaultAccessPolicy(id_, this));
}

ViewManagerServiceImpl::~ViewManagerServiceImpl() {
  // Delete any views we created.
  if (!view_map_.empty()) {
    ConnectionManager::ScopedChange change(this, connection_manager_, true);
    while (!view_map_.empty())
      delete view_map_.begin()->second;
  }

  connection_manager_->RemoveConnection(this);
}

const ServerView* ViewManagerServiceImpl::GetView(const ViewId& id) const {
  if (id_ == id.connection_id) {
    ViewMap::const_iterator i = view_map_.find(id.view_id);
    return i == view_map_.end() ? NULL : i->second;
  }
  return connection_manager_->GetView(id);
}

bool ViewManagerServiceImpl::HasRoot(const ViewId& id) const {
  return roots_.find(ViewIdToTransportId(id)) != roots_.end();
}

void ViewManagerServiceImpl::OnViewManagerServiceImplDestroyed(
    ConnectionSpecificId id) {
  if (creator_id_ == id)
    creator_id_ = kInvalidConnectionId;
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

void ViewManagerServiceImpl::ProcessWillChangeViewHierarchy(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent,
    bool originated_change) {
  if (originated_change)
    return;

  const bool old_drawn = view->IsDrawn(connection_manager_->root());
  const bool new_drawn = view->visible() && new_parent &&
      new_parent->IsDrawn(connection_manager_->root());
  if (old_drawn == new_drawn)
    return;

  NotifyDrawnStateChanged(view, new_drawn);
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
  view_map_.erase(view.view_id);

  const bool in_known = known_views_.erase(ViewIdToTransportId(view)) > 0;
  roots_.erase(ViewIdToTransportId(view));

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
    view_target_drawn_state =
        view->parent() && view->parent()->IsDrawn(connection_manager_->root());
  }

  NotifyDrawnStateChanged(view, view_target_drawn_state);
}

void ViewManagerServiceImpl::OnConnectionError() {
  if (delete_on_connection_error_)
    delete this;
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
  if ((direction == ORDER_DIRECTION_ABOVE && child_i == target_i + 1) ||
      (direction == ORDER_DIRECTION_BELOW && child_i + 1 == target_i)) {
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

void ViewManagerServiceImpl::RemoveRoot(const ViewId& view_id) {
  const Id transport_view_id(ViewIdToTransportId(view_id));
  CHECK(roots_.count(transport_view_id) > 0);

  roots_.erase(transport_view_id);

  // No need to do anything if we created the view.
  if (view_id.connection_id == id_)
    return;

  client()->OnViewDeleted(transport_view_id);
  connection_manager_->OnConnectionMessagedClient(id_);

  // This connection no longer knows about the view. Unparent any views that
  // were parented to views in the root.
  std::vector<ServerView*> local_views;
  RemoveFromKnown(GetView(view_id), &local_views);
  for (size_t i = 0; i < local_views.size(); ++i)
    local_views[i]->parent()->Remove(local_views[i]);
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
  ViewDataPtr view_data(ViewData::New());
  view_data->parent_id = ViewIdToTransportId(parent ? parent->id() : ViewId());
  view_data->view_id = ViewIdToTransportId(view->id());
  view_data->bounds = Rect::From(view->bounds());
  view_data->visible = view->visible();
  view_data->drawn = view->IsDrawn(connection_manager_->root());
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
  // Even though we don't know about view, it may be an ancestor of one of our
  // roots, in which case the change may effect our roots drawn state.
  for (ViewIdSet::iterator i = roots_.begin(); i != roots_.end(); ++i) {
    const ServerView* root = GetView(ViewIdFromTransportId(*i));
    DCHECK(root);
    if (view->Contains(root) &&
        (new_drawn_value != root->IsDrawn(connection_manager_->root()))) {
      client()->OnViewDrawnStateChanged(ViewIdToTransportId(root->id()),
                                        new_drawn_value);
    }
  }
}

void ViewManagerServiceImpl::CreateView(
    Id transport_view_id,
    const Callback<void(ErrorCode)>& callback) {
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  ErrorCode error_code = ERROR_CODE_NONE;
  if (view_id.connection_id != id_) {
    error_code = ERROR_CODE_ILLEGAL_ARGUMENT;
  } else if (view_map_.find(view_id.view_id) != view_map_.end()) {
    error_code = ERROR_CODE_VALUE_IN_USE;
  } else {
    view_map_[view_id.view_id] = new ServerView(connection_manager_, view_id);
    known_views_.insert(transport_view_id);
  }
  callback.Run(error_code);
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
  bool success = false;
  ServerView* parent = GetView(ViewIdFromTransportId(parent_id));
  ServerView* child = GetView(ViewIdFromTransportId(child_id));
  if (parent && child && child->parent() != parent &&
      !child->Contains(parent) && access_policy_->CanAddView(parent, child)) {
    success = true;
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    parent->Add(child);
  }
  callback.Run(success);
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
  ServerView* view = GetView(ViewIdFromTransportId(view_id));
  std::vector<const ServerView*> views;
  if (view) {
    GetViewTreeImpl(view, &views);
    // TODO(sky): this should map in views that weren't none.
  }
  callback.Run(ViewsToViewDatas(views));
}

void ViewManagerServiceImpl::SetViewSurfaceId(
    Id view_id,
    SurfaceIdPtr surface_id,
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
    RectPtr bounds,
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
  ServerView* view = GetView(ViewIdFromTransportId(transport_view_id));
  if (!view || view->visible() == visible ||
      !access_policy_->CanChangeViewVisibility(view)) {
    callback.Run(false);
    return;
  }
  {
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    view->SetVisible(visible);
  }
  callback.Run(true);
}

void ViewManagerServiceImpl::Embed(
    const String& url,
    Id transport_view_id,
    ServiceProviderPtr service_provider,
    const Callback<void(bool)>& callback) {
  InterfaceRequest<ServiceProvider> spir;
  spir.Bind(service_provider.PassMessagePipe());

  if (ViewIdFromTransportId(transport_view_id) == InvalidViewId()) {
    connection_manager_->EmbedRoot(url, spir.Pass());
    callback.Run(true);
    return;
  }
  const ServerView* view = GetView(ViewIdFromTransportId(transport_view_id));
  if (!view || !access_policy_->CanEmbed(view)) {
    callback.Run(false);
    return;
  }

  // Only allow a node to be the root for one connection.
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  ViewManagerServiceImpl* existing_owner =
      connection_manager_->GetConnectionWithRoot(view_id);

  ConnectionManager::ScopedChange change(this, connection_manager_, true);
  RemoveChildrenAsPartOfEmbed(view_id);
  if (existing_owner) {
    // Never message the originating connection.
    connection_manager_->OnConnectionMessagedClient(id_);
    existing_owner->RemoveRoot(view_id);
  }
  connection_manager_->Embed(id_, url, transport_view_id, spir.Pass());
  callback.Run(true);
}

void ViewManagerServiceImpl::DispatchOnViewInputEvent(Id transport_view_id,
                                                      EventPtr event) {
  // We only allow the WM to dispatch events. At some point this function will
  // move to a separate interface and the check can go away.
  if (id_ != kWindowManagerConnection)
    return;

  const ViewId view_id(ViewIdFromTransportId(transport_view_id));

  // If another app is embedded at this view, we forward the input event to the
  // embedded app, rather than the app that created the view.
  ViewManagerServiceImpl* connection =
      connection_manager_->GetConnectionWithRoot(view_id);
  if (!connection)
    connection = connection_manager_->GetConnection(view_id.connection_id);
  if (connection) {
    connection->client()->OnViewInputEvent(
        transport_view_id,
        event.Pass(),
        base::Bind(&base::DoNothing));
  }
}

void ViewManagerServiceImpl::OnConnectionEstablished() {
  connection_manager_->AddConnection(this);

  std::vector<const ServerView*> to_send;
  for (ViewIdSet::const_iterator i = roots_.begin(); i != roots_.end(); ++i)
    GetUnknownViewsFrom(GetView(ViewIdFromTransportId(*i)), &to_send);

  client()->OnEmbed(id_,
                    creator_url_,
                    ViewToViewData(to_send.front()),
                    service_provider_.Pass());
}

const base::hash_set<Id>&
ViewManagerServiceImpl::GetRootsForAccessPolicy() const {
  return roots_;
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

}  // namespace service
}  // namespace mojo
