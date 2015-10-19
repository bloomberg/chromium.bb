// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/connection_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "components/mus/ws/window_tree_host_connection.h"
#include "components/mus/ws/window_tree_impl.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace mus {

ConnectionManager::ScopedChange::ScopedChange(
    WindowTreeImpl* connection,
    ConnectionManager* connection_manager,
    bool is_delete_window)
    : connection_manager_(connection_manager),
      connection_id_(connection->id()),
      is_delete_window_(is_delete_window) {
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

void ConnectionManager::AddHost(WindowTreeHostConnection* host_connection) {
  DCHECK_EQ(0u,
            host_connection_map_.count(host_connection->window_tree_host()));
  host_connection_map_[host_connection->window_tree_host()] = host_connection;
}

ServerWindow* ConnectionManager::CreateServerWindow(const WindowId& id) {
  ServerWindow* window = new ServerWindow(this, id);
  window->AddObserver(this);
  return window;
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
  const WindowId* window_id = connection->service()->root();
  ServerWindow* window =
      window_id ? GetWindow(*connection->service()->root()) : nullptr;
  // If the WindowTree root is a viewport root, then we'll wait until
  // the root connection goes away to cleanup.
  if (window && (GetRootWindow(window) == window))
    return;

  scoped_ptr<ClientConnection> connection_owner(connection);

  connection_map_.erase(connection->service()->id());

  // Notify remaining connections so that they can cleanup.
  for (auto& pair : connection_map_) {
    pair.second->service()->OnWillDestroyWindowTreeImpl(connection->service());
  }
}

void ConnectionManager::OnHostConnectionClosed(
    WindowTreeHostConnection* connection) {
  auto it = host_connection_map_.find(connection->window_tree_host());
  DCHECK(it != host_connection_map_.end());

  // Get the ClientConnection by WindowTreeImpl ID.
  ConnectionMap::iterator service_connection_it =
      connection_map_.find(it->first->GetWindowTree()->id());
  DCHECK(service_connection_it != connection_map_.end());

  // Tear down the associated WindowTree connection.
  // TODO(fsamuel): I don't think this is quite right, we should tear down all
  // connections within the root's viewport. We should probably employ an
  // observer pattern to do this. Each WindowTreeImpl should track its
  // parent's lifetime.
  host_connection_map_.erase(it);
  OnConnectionError(service_connection_it->second);

  // If we have no more roots left, let the app know so it can terminate.
  if (!host_connection_map_.size())
    delegate_->OnNoMoreRootConnections();
}

void ConnectionManager::EmbedAtWindow(ConnectionSpecificId creator_id,
                                      const WindowId& window_id,
                                      uint32_t policy_bitmask,
                                      mojo::URLRequestPtr request) {
  mojom::WindowTreePtr service_ptr;
  ClientConnection* client_connection =
      delegate_->CreateClientConnectionForEmbedAtWindow(
          this, GetProxy(&service_ptr), creator_id, request.Pass(), window_id,
          policy_bitmask);
  AddConnection(client_connection);
  client_connection->service()->Init(client_connection->client(),
                                     service_ptr.Pass());
  OnConnectionMessagedClient(client_connection->service()->id());
}

WindowTreeImpl* ConnectionManager::EmbedAtWindow(
    ConnectionSpecificId creator_id,
    const WindowId& window_id,
    uint32_t policy_bitmask,
    mojom::WindowTreeClientPtr client) {
  mojom::WindowTreePtr service_ptr;
  ClientConnection* client_connection =
      delegate_->CreateClientConnectionForEmbedAtWindow(
          this, GetProxy(&service_ptr), creator_id, window_id, policy_bitmask,
          client.Pass());
  AddConnection(client_connection);
  client_connection->service()->Init(client_connection->client(),
                                     service_ptr.Pass());
  OnConnectionMessagedClient(client_connection->service()->id());

  return client_connection->service();
}

WindowTreeImpl* ConnectionManager::GetConnection(
    ConnectionSpecificId connection_id) {
  ConnectionMap::iterator i = connection_map_.find(connection_id);
  return i == connection_map_.end() ? nullptr : i->second->service();
}

ServerWindow* ConnectionManager::GetWindow(const WindowId& id) {
  for (auto& pair : host_connection_map_) {
    if (pair.first->root_window()->id() == id)
      return pair.first->root_window();
  }
  WindowTreeImpl* service = GetConnection(id.connection_id);
  return service ? service->GetWindow(id) : nullptr;
}

bool ConnectionManager::IsWindowAttachedToRoot(
    const ServerWindow* window) const {
  for (auto& pair : host_connection_map_) {
    if (pair.first->IsWindowAttachedToRoot(window))
      return true;
  }
  return false;
}

void ConnectionManager::SchedulePaint(const ServerWindow* window,
                                      const gfx::Rect& bounds) {
  for (auto& pair : host_connection_map_) {
    if (pair.first->SchedulePaintIfInViewport(window, bounds))
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

mojom::ViewportMetricsPtr ConnectionManager::GetViewportMetricsForWindow(
    const ServerWindow* window) {
  WindowTreeHostImpl* host = GetWindowTreeHostByWindow(window);
  if (host)
    return host->GetViewportMetrics().Clone();

  if (!host_connection_map_.empty())
    return host_connection_map_.begin()->first->GetViewportMetrics().Clone();

  mojom::ViewportMetricsPtr metrics = mojom::ViewportMetrics::New();
  metrics->size_in_pixels = mojo::Size::New();
  return metrics.Pass();
}

const WindowTreeImpl* ConnectionManager::GetConnectionWithRoot(
    const WindowId& id) const {
  for (auto& pair : connection_map_) {
    if (pair.second->service()->IsRoot(id))
      return pair.second->service();
  }
  return nullptr;
}

WindowTreeHostImpl* ConnectionManager::GetWindowTreeHostByWindow(
    const ServerWindow* window) {
  return const_cast<WindowTreeHostImpl*>(
      static_cast<const ConnectionManager*>(this)
          ->GetWindowTreeHostByWindow(window));
}

const WindowTreeHostImpl* ConnectionManager::GetWindowTreeHostByWindow(
    const ServerWindow* window) const {
  while (window && window->parent())
    window = window->parent();
  for (auto& pair : host_connection_map_) {
    if (window == pair.first->root_window())
      return pair.first;
  }
  return nullptr;
}

WindowTreeImpl* ConnectionManager::GetEmbedRoot(WindowTreeImpl* service) {
  while (service) {
    const WindowId* root_id = service->root();
    if (!root_id || root_id->connection_id == service->id())
      return nullptr;

    WindowTreeImpl* parent_service = GetConnection(root_id->connection_id);
    service = parent_service;
    if (service && service->is_embed_root())
      return service;
  }
  return nullptr;
}

void ConnectionManager::ProcessWindowBoundsChanged(
    const ServerWindow* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWindowBoundsChanged(
        window, old_bounds, new_bounds, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessClientAreaChanged(
    const ServerWindow* window,
    const gfx::Rect& old_client_area,
    const gfx::Rect& new_client_area) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessClientAreaChanged(
        window, old_client_area, new_client_area, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessWillChangeWindowHierarchy(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWillChangeWindowHierarchy(
        window, new_parent, old_parent, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessWindowHierarchyChanged(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWindowHierarchyChanged(
        window, new_parent, old_parent, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessWindowReorder(
    const ServerWindow* window,
    const ServerWindow* relative_window,
    const mojom::OrderDirection direction) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWindowReorder(
        window, relative_window, direction, IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessWindowDeleted(const WindowId& window) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWindowDeleted(window,
                                                 IsChangeSource(pair.first));
  }
}

void ConnectionManager::ProcessViewportMetricsChanged(
    const mojom::ViewportMetrics& old_metrics,
    const mojom::ViewportMetrics& new_metrics) {
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

void ConnectionManager::OnScheduleWindowPaint(const ServerWindow* window) {
  if (!in_destructor_)
    SchedulePaint(window, gfx::Rect(window->bounds().size()));
}

const ServerWindow* ConnectionManager::GetRootWindow(
    const ServerWindow* window) const {
  const WindowTreeHostImpl* host = GetWindowTreeHostByWindow(window);
  return host ? host->root_window() : nullptr;
}

void ConnectionManager::OnWindowDestroyed(ServerWindow* window) {
  if (!in_destructor_)
    ProcessWindowDeleted(window->id());
}

void ConnectionManager::OnWillChangeWindowHierarchy(ServerWindow* window,
                                                    ServerWindow* new_parent,
                                                    ServerWindow* old_parent) {
  if (in_destructor_)
    return;

  ProcessWillChangeWindowHierarchy(window, new_parent, old_parent);
}

void ConnectionManager::OnWindowHierarchyChanged(ServerWindow* window,
                                                 ServerWindow* new_parent,
                                                 ServerWindow* old_parent) {
  if (in_destructor_)
    return;

  ProcessWindowHierarchyChanged(window, new_parent, old_parent);

  // TODO(beng): optimize.
  if (old_parent)
    SchedulePaint(old_parent, gfx::Rect(old_parent->bounds().size()));
  if (new_parent)
    SchedulePaint(new_parent, gfx::Rect(new_parent->bounds().size()));
}

void ConnectionManager::OnWindowBoundsChanged(ServerWindow* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  if (in_destructor_)
    return;

  ProcessWindowBoundsChanged(window, old_bounds, new_bounds);
  if (!window->parent())
    return;

  // TODO(sky): optimize this.
  SchedulePaint(window->parent(), old_bounds);
  SchedulePaint(window->parent(), new_bounds);
}

void ConnectionManager::OnWindowClientAreaChanged(
    ServerWindow* window,
    const gfx::Rect& old_client_area,
    const gfx::Rect& new_client_area) {
  if (in_destructor_)
    return;

  ProcessClientAreaChanged(window, old_client_area, new_client_area);
}

void ConnectionManager::OnWindowReordered(ServerWindow* window,
                                          ServerWindow* relative,
                                          mojom::OrderDirection direction) {
  if (!in_destructor_)
    SchedulePaint(window, gfx::Rect(window->bounds().size()));
}

void ConnectionManager::OnWillChangeWindowVisibility(ServerWindow* window) {
  if (in_destructor_)
    return;

  // Need to repaint if the window was drawn (which means it's in the process of
  // hiding) or the window is transitioning to drawn.
  if (window->parent() &&
      (window->IsDrawn() ||
       (!window->visible() && window->parent()->IsDrawn()))) {
    SchedulePaint(window->parent(), window->bounds());
  }

  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWillChangeWindowVisibility(
        window, IsChangeSource(pair.first));
  }
}

void ConnectionManager::OnWindowSharedPropertyChanged(
    ServerWindow* window,
    const std::string& name,
    const std::vector<uint8_t>* new_data) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWindowPropertyChanged(
        window, name, new_data, IsChangeSource(pair.first));
  }
}

void ConnectionManager::OnWindowTextInputStateChanged(
    ServerWindow* window,
    const ui::TextInputState& state) {
  WindowTreeHostImpl* host = GetWindowTreeHostByWindow(window);
  host->UpdateTextInputState(window, state);
}

}  // namespace mus
