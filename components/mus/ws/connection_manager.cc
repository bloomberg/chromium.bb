// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/connection_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/server_view.h"
#include "components/mus/ws/view_coordinate_conversions.h"
#include "components/mus/ws/view_tree_host_connection.h"
#include "components/mus/ws/view_tree_impl.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace mus {

ConnectionManager::ScopedChange::ScopedChange(
    ViewTreeImpl* connection,
    ConnectionManager* connection_manager,
    bool is_delete_view)
    : connection_manager_(connection_manager),
      connection_id_(connection->id()),
      is_delete_view_(is_delete_view) {
  connection_manager_->PrepareForChange(this);
}

ConnectionManager::ScopedChange::~ScopedChange() {
  connection_manager_->FinishChange();
}

ConnectionManager::ConnectionManager(
    ConnectionManagerDelegate* delegate,
    const scoped_refptr<SurfacesState>& surfaces_state)
    : delegate_(delegate),
      surfaces_state_(surfaces_state),
      next_connection_id_(1),
      next_host_id_(0),
      current_change_(nullptr),
      in_destructor_(false) {}

ConnectionManager::~ConnectionManager() {
  in_destructor_ = true;

  // Copy the HostConnectionMap because it will be mutated as the connections
  // are closed.
  HostConnectionMap host_connection_map(host_connection_map_);
  for (auto& pair : host_connection_map)
    pair.second->CloseConnection();

  STLDeleteValues(&connection_map_);
  // All the connections should have been destroyed.
  DCHECK(host_connection_map_.empty());
  DCHECK(connection_map_.empty());
}

void ConnectionManager::AddHost(ViewTreeHostConnection* host_connection) {
  DCHECK_EQ(0u, host_connection_map_.count(host_connection->view_tree_host()));
  host_connection_map_[host_connection->view_tree_host()] = host_connection;
}

ServerView* ConnectionManager::CreateServerView(const ViewId& id) {
  ServerView* view = new ServerView(this, id);
  view->AddObserver(this);
  return view;
}

ConnectionSpecificId ConnectionManager::GetAndAdvanceNextConnectionId() {
  const ConnectionSpecificId id = next_connection_id_++;
  DCHECK_LT(id, next_connection_id_);
  return id;
}

uint16_t ConnectionManager::GetAndAdvanceNextHostId() {
  const uint16_t id = next_host_id_++;
  DCHECK_LT(id, next_host_id_);
  return id;
}

void ConnectionManager::OnConnectionError(ClientConnection* connection) {
  // This will be null if the root has been destroyed.
  const ViewId* view_id = connection->service()->root();
  ServerView* view =
      view_id ? GetView(*connection->service()->root()) : nullptr;
  // If the ViewTree root is a viewport root, then we'll wait until
  // the root connection goes away to cleanup.
  if (view && (GetRootView(view) == view))
    return;

  scoped_ptr<ClientConnection> connection_owner(connection);

  connection_map_.erase(connection->service()->id());

  // Notify remaining connections so that they can cleanup.
  for (auto& pair : connection_map_) {
    pair.second->service()->OnWillDestroyViewTreeImpl(connection->service());
  }
}

void ConnectionManager::OnHostConnectionClosed(
    ViewTreeHostConnection* connection) {
  auto it = host_connection_map_.find(connection->view_tree_host());
  DCHECK(it != host_connection_map_.end());

  // Get the ClientConnection by ViewTreeImpl ID.
  ConnectionMap::iterator service_connection_it =
      connection_map_.find(it->first->GetViewTree()->id());
  DCHECK(service_connection_it != connection_map_.end());

  // Tear down the associated ViewTree connection.
  // TODO(fsamuel): I don't think this is quite right, we should tear down all
  // connections within the root's viewport. We should probably employ an
  // observer pattern to do this. Each ViewTreeImpl should track its
  // parent's lifetime.
  host_connection_map_.erase(it);
  OnConnectionError(service_connection_it->second);

  // If we have no more roots left, let the app know so it can terminate.
  if (!host_connection_map_.size())
    delegate_->OnNoMoreRootConnections();
}

void ConnectionManager::EmbedAtView(ConnectionSpecificId creator_id,
                                    const ViewId& view_id,
                                    uint32_t policy_bitmask,
                                    mojo::URLRequestPtr request) {
  mojo::ViewTreePtr service_ptr;
  ClientConnection* client_connection =
      delegate_->CreateClientConnectionForEmbedAtView(
          this, GetProxy(&service_ptr), creator_id, request.Pass(), view_id,
          policy_bitmask);
  AddConnection(client_connection);
  client_connection->service()->Init(client_connection->client(),
                                     service_ptr.Pass());
  OnConnectionMessagedClient(client_connection->service()->id());
}

ViewTreeImpl* ConnectionManager::EmbedAtView(ConnectionSpecificId creator_id,
                                             const ViewId& view_id,
                                             uint32_t policy_bitmask,
                                             mojo::ViewTreeClientPtr client) {
  mojo::ViewTreePtr service_ptr;
  ClientConnection* client_connection =
      delegate_->CreateClientConnectionForEmbedAtView(
          this, GetProxy(&service_ptr), creator_id, view_id, policy_bitmask,
          client.Pass());
  AddConnection(client_connection);
  client_connection->service()->Init(client_connection->client(),
                                     service_ptr.Pass());
  OnConnectionMessagedClient(client_connection->service()->id());

  return client_connection->service();
}

ViewTreeImpl* ConnectionManager::GetConnection(
    ConnectionSpecificId connection_id) {
  ConnectionMap::iterator i = connection_map_.find(connection_id);
  return i == connection_map_.end() ? nullptr : i->second->service();
}

ServerView* ConnectionManager::GetView(const ViewId& id) {
  for (auto& pair : host_connection_map_) {
    if (pair.first->root_view()->id() == id)
      return pair.first->root_view();
  }
  ViewTreeImpl* service = GetConnection(id.connection_id);
  return service ? service->GetView(id) : nullptr;
}

bool ConnectionManager::IsViewAttachedToRoot(const ServerView* view) const {
  for (auto& pair : host_connection_map_) {
    if (pair.first->IsViewAttachedToRoot(view))
      return true;
  }
  return false;
}

void ConnectionManager::SchedulePaint(const ServerView* view,
                                      const gfx::Rect& bounds) {
  for (auto& pair : host_connection_map_) {
    if (pair.first->SchedulePaintIfInViewport(view, bounds))
      return;
  }
}

void ConnectionManager::OnConnectionMessagedClient(ConnectionSpecificId id) {
  if (current_change_)
    current_change_->MarkConnectionAsMessaged(id);
}

bool ConnectionManager::DidConnectionMessageClient(
    ConnectionSpecificId id) const {
  return current_change_ && current_change_->DidMessageConnection(id);
}

mojo::ViewportMetricsPtr ConnectionManager::GetViewportMetricsForView(
    const ServerView* view) {
  ViewTreeHostImpl* host = GetViewTreeHostByView(view);
  if (host)
    return host->GetViewportMetrics().Clone();

  if (!host_connection_map_.empty())
    return host_connection_map_.begin()->first->GetViewportMetrics().Clone();

  mojo::ViewportMetricsPtr metrics = mojo::ViewportMetrics::New();
  metrics->size_in_pixels = mojo::Size::New();
  return metrics.Pass();
}

const ViewTreeImpl* ConnectionManager::GetConnectionWithRoot(
    const ViewId& id) const {
  for (auto& pair : connection_map_) {
    if (pair.second->service()->IsRoot(id))
      return pair.second->service();
  }
  return nullptr;
}

ViewTreeHostImpl* ConnectionManager::GetViewTreeHostByView(
    const ServerView* view) {
  return const_cast<ViewTreeHostImpl*>(
      static_cast<const ConnectionManager*>(this)->GetViewTreeHostByView(view));
}

const ViewTreeHostImpl* ConnectionManager::GetViewTreeHostByView(
    const ServerView* view) const {
  while (view && view->parent())
    view = view->parent();
  for (auto& pair : host_connection_map_) {
    if (view == pair.first->root_view())
      return pair.first;
  }
  return nullptr;
}

ViewTreeImpl* ConnectionManager::GetEmbedRoot(ViewTreeImpl* service) {
  while (service) {
    const ViewId* root_id = service->root();
    if (!root_id || root_id->connection_id == service->id())
      return nullptr;

    ViewTreeImpl* parent_service = GetConnection(root_id->connection_id);
    service = parent_service;
    if (service && service->is_embed_root())
      return service;
  }
  return nullptr;
}

void ConnectionManager::ProcessViewBoundsChanged(const ServerView* view,
                                                 const gfx::Rect& old_bounds,
                                                 const gfx::Rect& new_bounds) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewBoundsChanged(
        view, old_bounds, new_bounds, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessWillChangeViewHierarchy(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWillChangeViewHierarchy(
        view, new_parent, old_parent, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessViewHierarchyChanged(
    const ServerView* view,
    const ServerView* new_parent,
    const ServerView* old_parent) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewHierarchyChanged(
        view, new_parent, old_parent, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessViewReorder(
    const ServerView* view,
    const ServerView* relative_view,
    const mojo::OrderDirection direction) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewReorder(view, relative_view, direction,
                                               IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessViewDeleted(const ViewId& view) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewDeleted(view,
                                               IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessViewportMetricsChanged(
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewportMetricsChanged(
        old_metrics, new_metrics, IsChangeSource(pair.first));
  }
}

void ConnectionManager::PrepareForChange(ScopedChange* change) {
  // Should only ever have one change in flight.
  CHECK(!current_change_);
  current_change_ = change;
}

void ConnectionManager::FinishChange() {
  // PrepareForChange/FinishChange should be balanced.
  CHECK(current_change_);
  current_change_ = NULL;
}

void ConnectionManager::AddConnection(ClientConnection* connection) {
  DCHECK_EQ(0u, connection_map_.count(connection->service()->id()));
  connection_map_[connection->service()->id()] = connection;
}

SurfacesState* ConnectionManager::GetSurfacesState() {
  return surfaces_state_.get();
}

void ConnectionManager::OnScheduleViewPaint(const ServerView* view) {
  if (!in_destructor_)
    SchedulePaint(view, gfx::Rect(view->bounds().size()));
}

const ServerView* ConnectionManager::GetRootView(const ServerView* view) const {
  const ViewTreeHostImpl* host = GetViewTreeHostByView(view);
  return host ? host->root_view() : nullptr;
}

void ConnectionManager::OnViewDestroyed(ServerView* view) {
  if (!in_destructor_)
    ProcessViewDeleted(view->id());
}

void ConnectionManager::OnWillChangeViewHierarchy(ServerView* view,
                                                  ServerView* new_parent,
                                                  ServerView* old_parent) {
  if (in_destructor_)
    return;

  ProcessWillChangeViewHierarchy(view, new_parent, old_parent);
}

void ConnectionManager::OnViewHierarchyChanged(ServerView* view,
                                               ServerView* new_parent,
                                               ServerView* old_parent) {
  if (in_destructor_)
    return;

  ProcessViewHierarchyChanged(view, new_parent, old_parent);

  // TODO(beng): optimize.
  if (old_parent)
    SchedulePaint(old_parent, gfx::Rect(old_parent->bounds().size()));
  if (new_parent)
    SchedulePaint(new_parent, gfx::Rect(new_parent->bounds().size()));
}

void ConnectionManager::OnViewBoundsChanged(ServerView* view,
                                            const gfx::Rect& old_bounds,
                                            const gfx::Rect& new_bounds) {
  if (in_destructor_)
    return;

  ProcessViewBoundsChanged(view, old_bounds, new_bounds);
  if (!view->parent())
    return;

  // TODO(sky): optimize this.
  SchedulePaint(view->parent(), old_bounds);
  SchedulePaint(view->parent(), new_bounds);
}

void ConnectionManager::OnViewReordered(ServerView* view,
                                        ServerView* relative,
                                        mojo::OrderDirection direction) {
  if (!in_destructor_)
    SchedulePaint(view, gfx::Rect(view->bounds().size()));
}

void ConnectionManager::OnWillChangeViewVisibility(ServerView* view) {
  if (in_destructor_)
    return;

  // Need to repaint if the view was drawn (which means it's in the process of
  // hiding) or the view is transitioning to drawn.
  if (view->parent() &&
      (view->IsDrawn() || (!view->visible() && view->parent()->IsDrawn()))) {
    SchedulePaint(view->parent(), view->bounds());
  }

  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWillChangeViewVisibility(
        view, IsChangeSource(pair.first));
  }
}

void ConnectionManager::OnViewSharedPropertyChanged(
    ServerView* view,
    const std::string& name,
    const std::vector<uint8_t>* new_data) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewPropertyChanged(
        view, name, new_data, IsChangeSource(pair.first));
  }
}

void ConnectionManager::OnViewTextInputStateChanged(
    ServerView* view,
    const ui::TextInputState& state) {
  ViewTreeHostImpl* host = GetViewTreeHostByView(view);
  host->UpdateTextInputState(view, state);
}

}  // namespace mus
