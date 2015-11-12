// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/connection_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/operation.h"
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

namespace ws {

ConnectionManager::ConnectionManager(
    ConnectionManagerDelegate* delegate,
    const scoped_refptr<mus::SurfacesState>& surfaces_state)
    : delegate_(delegate),
      surfaces_state_(surfaces_state),
      next_connection_id_(1),
      next_host_id_(0),
      current_operation_(nullptr),
      in_destructor_(false),
      next_wm_change_id_(0) {}

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
  for (auto& pair : connection_map_)
    pair.second->service()->OnWillDestroyWindowTreeImpl(connection->service());

  // Remove any requests from the client that resulted in a call to the window
  // manager and we haven't gotten a response back yet.
  std::set<uint32_t> to_remove;
  for (auto& pair : in_flight_wm_change_map_) {
    if (pair.second.connection_id == connection->service()->id())
      to_remove.insert(pair.first);
  }
  for (uint32_t id : to_remove)
    in_flight_wm_change_map_.erase(id);
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
  if (current_operation_)
    current_operation_->MarkConnectionAsMessaged(id);
}

bool ConnectionManager::DidConnectionMessageClient(
    ConnectionSpecificId id) const {
  return current_operation_ && current_operation_->DidMessageConnection(id);
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

uint32_t ConnectionManager::GenerateWindowManagerChangeId(
    WindowTreeImpl* source,
    uint32_t client_change_id) {
  const uint32_t wm_change_id = next_wm_change_id_++;
  in_flight_wm_change_map_[wm_change_id] = {source->id(), client_change_id};
  return wm_change_id;
}

void ConnectionManager::WindowManagerChangeCompleted(
    uint32_t window_manager_change_id,
    bool success) {
  // There are valid reasons as to why we wouldn't know about the id. The
  // most likely is the client disconnected before the response from the window
  // manager came back.
  auto iter = in_flight_wm_change_map_.find(window_manager_change_id);
  if (iter == in_flight_wm_change_map_.end())
    return;

  const InFlightWindowManagerChange change = iter->second;
  in_flight_wm_change_map_.erase(iter);

  WindowTreeImpl* connection = GetConnection(change.connection_id);
  connection->client()->OnChangeCompleted(change.client_change_id, success);
}

void ConnectionManager::ProcessWindowBoundsChanged(
    const ServerWindow* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWindowBoundsChanged(
        window, old_bounds, new_bounds, IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessClientAreaChanged(
    const ServerWindow* window,
    const gfx::Insets& old_client_area,
    const gfx::Insets& new_client_area) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessClientAreaChanged(
        window, old_client_area, new_client_area,
        IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessWillChangeWindowHierarchy(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWillChangeWindowHierarchy(
        window, new_parent, old_parent, IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessWindowHierarchyChanged(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWindowHierarchyChanged(
        window, new_parent, old_parent, IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessWindowReorder(
    const ServerWindow* window,
    const ServerWindow* relative_window,
    const mojom::OrderDirection direction) {
  // We'll probably do a bit of reshuffling when we add a transient window.
  if ((current_operation_type() == OperationType::ADD_TRANSIENT_WINDOW) ||
      (current_operation_type() ==
       OperationType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT)) {
    return;
  }
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWindowReorder(
        window, relative_window, direction, IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessWindowDeleted(const WindowId& window) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWindowDeleted(window,
                                                 IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessViewportMetricsChanged(
    const mojom::ViewportMetrics& old_metrics,
    const mojom::ViewportMetrics& new_metrics) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessViewportMetricsChanged(
        old_metrics, new_metrics, IsOperationSource(pair.first));
  }
}

void ConnectionManager::PrepareForOperation(Operation* op) {
  // Should only ever have one change in flight.
  CHECK(!current_operation_);
  current_operation_ = op;
}

void ConnectionManager::FinishOperation() {
  // PrepareForOperation/FinishOperation should be balanced.
  CHECK(current_operation_);
  current_operation_ = nullptr;
}

void ConnectionManager::AddConnection(ClientConnection* connection) {
  DCHECK_EQ(0u, connection_map_.count(connection->service()->id()));
  connection_map_[connection->service()->id()] = connection;
}

mus::SurfacesState* ConnectionManager::GetSurfacesState() {
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

void ConnectionManager::ScheduleSurfaceDestruction(ServerWindow* window) {
  for (auto& pair : host_connection_map_) {
    if (pair.first->root_window()->Contains(window)) {
      pair.first->ScheduleSurfaceDestruction(window);
      break;
    }
  }
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
    const gfx::Insets& old_client_area,
    const gfx::Insets& new_client_area) {
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
        window, IsOperationSource(pair.first));
  }
}

void ConnectionManager::OnWindowSharedPropertyChanged(
    ServerWindow* window,
    const std::string& name,
    const std::vector<uint8_t>* new_data) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessWindowPropertyChanged(
        window, name, new_data, IsOperationSource(pair.first));
  }
}

void ConnectionManager::OnWindowTextInputStateChanged(
    ServerWindow* window,
    const ui::TextInputState& state) {
  WindowTreeHostImpl* host = GetWindowTreeHostByWindow(window);
  host->UpdateTextInputState(window, state);
}

void ConnectionManager::OnTransientWindowAdded(ServerWindow* window,
                                               ServerWindow* transient_child) {
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessTransientWindowAdded(
        window, transient_child, IsOperationSource(pair.first));
  }
}

void ConnectionManager::OnTransientWindowRemoved(
    ServerWindow* window,
    ServerWindow* transient_child) {
  // If we're deleting a window, then this is a superfluous message.
  if (current_operation_type() == OperationType::DELETE_WINDOW)
    return;
  for (auto& pair : connection_map_) {
    pair.second->service()->ProcessTransientWindowRemoved(
        window, transient_child, IsOperationSource(pair.first));
  }
}

}  // namespace ws

}  // namespace mus
