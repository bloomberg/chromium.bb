// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/default_access_policy.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/window_manager_access_policy.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/ime/ime_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "ui/platform_window/text_input_state.h"

using mojo::Array;
using mojo::Callback;
using mojo::InterfaceRequest;
using mojo::Rect;
using mojo::ServiceProvider;
using mojo::ServiceProviderPtr;
using mojo::String;

namespace mus {

WindowTreeImpl::WindowTreeImpl(ConnectionManager* connection_manager,
                               ConnectionSpecificId creator_id,
                               const WindowId& root_id,
                               uint32_t policy_bitmask)
    : connection_manager_(connection_manager),
      id_(connection_manager_->GetAndAdvanceNextConnectionId()),
      creator_id_(creator_id),
      client_(nullptr),
      is_embed_root_(false) {
  ServerWindow* window = GetWindow(root_id);
  CHECK(window);
  root_.reset(new WindowId(root_id));
  if (window->GetRoot() == window) {
    access_policy_.reset(new WindowManagerAccessPolicy(id_, this));
    is_embed_root_ = true;
  } else {
    access_policy_.reset(new DefaultAccessPolicy(id_, this));
    is_embed_root_ =
        (policy_bitmask & WindowTree::ACCESS_POLICY_EMBED_ROOT) != 0;
  }
}

WindowTreeImpl::~WindowTreeImpl() {
  DestroyWindows();
}

void WindowTreeImpl::Init(mojom::WindowTreeClient* client,
                          mojom::WindowTreePtr tree) {
  DCHECK(!client_);
  client_ = client;
  std::vector<const ServerWindow*> to_send;
  if (root_.get())
    GetUnknownWindowsFrom(GetWindow(*root_), &to_send);

  // TODO(beng): verify that host can actually be nullptr here.
  WindowTreeHostImpl* host = GetHost();
  const ServerWindow* focused_window =
      host ? host->GetFocusedWindow() : nullptr;
  if (focused_window)
    focused_window = access_policy_->GetWindowForFocusChange(focused_window);
  const Id focused_window_transport_id(WindowIdToTransportId(
      focused_window ? focused_window->id() : WindowId()));

  client->OnEmbed(id_, WindowToWindowData(to_send.front()), tree.Pass(),
                  focused_window_transport_id,
                  is_embed_root_ ? WindowTree::ACCESS_POLICY_EMBED_ROOT
                                 : WindowTree::ACCESS_POLICY_DEFAULT);
}

const ServerWindow* WindowTreeImpl::GetWindow(const WindowId& id) const {
  if (id_ == id.connection_id) {
    WindowMap::const_iterator i = window_map_.find(id.window_id);
    return i == window_map_.end() ? NULL : i->second;
  }
  return connection_manager_->GetWindow(id);
}

bool WindowTreeImpl::IsRoot(const WindowId& id) const {
  return root_.get() && *root_ == id;
}

WindowTreeHostImpl* WindowTreeImpl::GetHost() {
  return root_.get()
             ? connection_manager_->GetWindowTreeHostByWindow(GetWindow(*root_))
             : nullptr;
}

void WindowTreeImpl::OnWillDestroyWindowTreeImpl(WindowTreeImpl* connection) {
  if (creator_id_ == connection->id())
    creator_id_ = kInvalidConnectionId;
  const ServerWindow* connection_root =
      connection->root_ ? connection->GetWindow(*connection->root_) : nullptr;
  if (connection_root &&
      ((connection_root->id().connection_id == id_ &&
        window_map_.count(connection_root->id().window_id) > 0) ||
       (is_embed_root_ && IsWindowKnown(connection_root)))) {
    client()->OnEmbeddedAppDisconnected(
        WindowIdToTransportId(*connection->root_));
  }
  if (root_.get() && root_->connection_id == connection->id())
    root_.reset();
}

mojom::ErrorCode WindowTreeImpl::NewWindow(const WindowId& window_id) {
  if (window_id.connection_id != id_)
    return mojom::ERROR_CODE_ILLEGAL_ARGUMENT;
  if (window_map_.find(window_id.window_id) != window_map_.end())
    return mojom::ERROR_CODE_VALUE_IN_USE;
  window_map_[window_id.window_id] =
      connection_manager_->CreateServerWindow(window_id);
  known_windows_.insert(WindowIdToTransportId(window_id));
  return mojom::ERROR_CODE_NONE;
}

bool WindowTreeImpl::AddWindow(const WindowId& parent_id,
                               const WindowId& child_id) {
  ServerWindow* parent = GetWindow(parent_id);
  ServerWindow* child = GetWindow(child_id);
  if (parent && child && child->parent() != parent &&
      !child->Contains(parent) && access_policy_->CanAddWindow(parent, child)) {
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    parent->Add(child);
    return true;
  }
  return false;
}

std::vector<const ServerWindow*> WindowTreeImpl::GetWindowTree(
    const WindowId& window_id) const {
  const ServerWindow* window = GetWindow(window_id);
  std::vector<const ServerWindow*> windows;
  if (window)
    GetWindowTreeImpl(window, &windows);
  return windows;
}

bool WindowTreeImpl::SetWindowVisibility(const WindowId& window_id,
                                         bool visible) {
  ServerWindow* window = GetWindow(window_id);
  if (!window || window->visible() == visible ||
      !access_policy_->CanChangeWindowVisibility(window)) {
    return false;
  }
  ConnectionManager::ScopedChange change(this, connection_manager_, false);
  window->SetVisible(visible);
  return true;
}

bool WindowTreeImpl::Embed(const WindowId& window_id,
                           mojom::WindowTreeClientPtr client,
                           uint32_t policy_bitmask,
                           ConnectionSpecificId* connection_id) {
  *connection_id = kInvalidConnectionId;
  if (!client.get() || !CanEmbed(window_id, policy_bitmask))
    return false;
  PrepareForEmbed(window_id);
  WindowTreeImpl* new_connection = connection_manager_->EmbedAtWindow(
      id_, window_id, policy_bitmask, client.Pass());
  if (is_embed_root_)
    *connection_id = new_connection->id();
  return true;
}

void WindowTreeImpl::Embed(const WindowId& window_id,
                           mojo::URLRequestPtr request) {
  if (!CanEmbed(window_id, WindowTree::ACCESS_POLICY_DEFAULT))
    return;
  PrepareForEmbed(window_id);
  connection_manager_->EmbedAtWindow(
      id_, window_id, mojom::WindowTree::ACCESS_POLICY_DEFAULT, request.Pass());
}

void WindowTreeImpl::ProcessWindowBoundsChanged(const ServerWindow* window,
                                                const gfx::Rect& old_bounds,
                                                const gfx::Rect& new_bounds,
                                                bool originated_change) {
  if (originated_change || !IsWindowKnown(window))
    return;
  client()->OnWindowBoundsChanged(WindowIdToTransportId(window->id()),
                                  Rect::From(old_bounds),
                                  Rect::From(new_bounds));
}

void WindowTreeImpl::ProcessClientAreaChanged(const ServerWindow* window,
                                              const gfx::Rect& old_client_area,
                                              const gfx::Rect& new_client_area,
                                              bool originated_change) {
  if (originated_change || !IsWindowKnown(window))
    return;
  client()->OnClientAreaChanged(WindowIdToTransportId(window->id()),
                                Rect::From(old_client_area),
                                Rect::From(new_client_area));
}

void WindowTreeImpl::ProcessViewportMetricsChanged(
    const mojom::ViewportMetrics& old_metrics,
    const mojom::ViewportMetrics& new_metrics,
    bool originated_change) {
  client()->OnWindowViewportMetricsChanged(old_metrics.Clone(),
                                           new_metrics.Clone());
}

void WindowTreeImpl::ProcessWillChangeWindowHierarchy(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent,
    bool originated_change) {
  if (originated_change)
    return;

  const bool old_drawn = window->IsDrawn();
  const bool new_drawn =
      window->visible() && new_parent && new_parent->IsDrawn();
  if (old_drawn == new_drawn)
    return;

  NotifyDrawnStateChanged(window, new_drawn);
}

void WindowTreeImpl::ProcessWindowPropertyChanged(
    const ServerWindow* window,
    const std::string& name,
    const std::vector<uint8_t>* new_data,
    bool originated_change) {
  if (originated_change)
    return;

  Array<uint8_t> data;
  if (new_data)
    data = Array<uint8_t>::From(*new_data);

  client()->OnWindowSharedPropertyChanged(WindowIdToTransportId(window->id()),
                                          String(name), data.Pass());
}

void WindowTreeImpl::ProcessWindowHierarchyChanged(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent,
    bool originated_change) {
  if (originated_change && !IsWindowKnown(window) && new_parent &&
      IsWindowKnown(new_parent)) {
    std::vector<const ServerWindow*> unused;
    GetUnknownWindowsFrom(window, &unused);
  }
  if (originated_change || connection_manager_->is_processing_delete_window() ||
      connection_manager_->DidConnectionMessageClient(id_)) {
    return;
  }

  if (!access_policy_->ShouldNotifyOnHierarchyChange(window, &new_parent,
                                                     &old_parent)) {
    return;
  }
  // Inform the client of any new windows and update the set of windows we know
  // about.
  std::vector<const ServerWindow*> to_send;
  if (!IsWindowKnown(window))
    GetUnknownWindowsFrom(window, &to_send);
  const WindowId new_parent_id(new_parent ? new_parent->id() : WindowId());
  const WindowId old_parent_id(old_parent ? old_parent->id() : WindowId());
  client()->OnWindowHierarchyChanged(
      WindowIdToTransportId(window->id()), WindowIdToTransportId(new_parent_id),
      WindowIdToTransportId(old_parent_id), WindowsToWindowDatas(to_send));
  connection_manager_->OnConnectionMessagedClient(id_);
}

void WindowTreeImpl::ProcessWindowReorder(const ServerWindow* window,
                                          const ServerWindow* relative_window,
                                          mojom::OrderDirection direction,
                                          bool originated_change) {
  if (originated_change || !IsWindowKnown(window) ||
      !IsWindowKnown(relative_window))
    return;

  client()->OnWindowReordered(WindowIdToTransportId(window->id()),
                              WindowIdToTransportId(relative_window->id()),
                              direction);
}

void WindowTreeImpl::ProcessWindowDeleted(const WindowId& window,
                                          bool originated_change) {
  if (window.connection_id == id_)
    window_map_.erase(window.window_id);

  const bool in_known = known_windows_.erase(WindowIdToTransportId(window)) > 0;

  if (IsRoot(window))
    root_.reset();

  if (originated_change)
    return;

  if (in_known) {
    client()->OnWindowDeleted(WindowIdToTransportId(window));
    connection_manager_->OnConnectionMessagedClient(id_);
  }
}

void WindowTreeImpl::ProcessWillChangeWindowVisibility(
    const ServerWindow* window,
    bool originated_change) {
  if (originated_change)
    return;

  if (IsWindowKnown(window)) {
    client()->OnWindowVisibilityChanged(WindowIdToTransportId(window->id()),
                                        !window->visible());
    return;
  }

  bool window_target_drawn_state;
  if (window->visible()) {
    // Window is being hidden, won't be drawn.
    window_target_drawn_state = false;
  } else {
    // Window is being shown. Window will be drawn if its parent is drawn.
    window_target_drawn_state = window->parent() && window->parent()->IsDrawn();
  }

  NotifyDrawnStateChanged(window, window_target_drawn_state);
}

void WindowTreeImpl::ProcessFocusChanged(
    const ServerWindow* old_focused_window,
    const ServerWindow* new_focused_window) {
  const ServerWindow* window =
      new_focused_window
          ? access_policy_->GetWindowForFocusChange(new_focused_window)
          : nullptr;
  client()->OnWindowFocused(window ? WindowIdToTransportId(window->id())
                                   : WindowIdToTransportId(WindowId()));
}

bool WindowTreeImpl::IsWindowKnown(const ServerWindow* window) const {
  return known_windows_.count(WindowIdToTransportId(window->id())) > 0;
}

bool WindowTreeImpl::CanReorderWindow(const ServerWindow* window,
                                      const ServerWindow* relative_window,
                                      mojom::OrderDirection direction) const {
  if (!window || !relative_window)
    return false;

  if (!window->parent() || window->parent() != relative_window->parent())
    return false;

  if (!access_policy_->CanReorderWindow(window, relative_window, direction))
    return false;

  std::vector<const ServerWindow*> children = window->parent()->GetChildren();
  const size_t child_i =
      std::find(children.begin(), children.end(), window) - children.begin();
  const size_t target_i =
      std::find(children.begin(), children.end(), relative_window) -
      children.begin();
  if ((direction == mojom::ORDER_DIRECTION_ABOVE && child_i == target_i + 1) ||
      (direction == mojom::ORDER_DIRECTION_BELOW && child_i + 1 == target_i)) {
    return false;
  }

  return true;
}

bool WindowTreeImpl::DeleteWindowImpl(WindowTreeImpl* source,
                                      ServerWindow* window) {
  DCHECK(window);
  DCHECK_EQ(window->id().connection_id, id_);
  ConnectionManager::ScopedChange change(source, connection_manager_, true);
  delete window;
  return true;
}

void WindowTreeImpl::GetUnknownWindowsFrom(
    const ServerWindow* window,
    std::vector<const ServerWindow*>* windows) {
  if (IsWindowKnown(window) || !access_policy_->CanGetWindowTree(window))
    return;
  windows->push_back(window);
  known_windows_.insert(WindowIdToTransportId(window->id()));
  if (!access_policy_->CanDescendIntoWindowForWindowTree(window))
    return;
  std::vector<const ServerWindow*> children(window->GetChildren());
  for (size_t i = 0; i < children.size(); ++i)
    GetUnknownWindowsFrom(children[i], windows);
}

void WindowTreeImpl::RemoveFromKnown(
    const ServerWindow* window,
    std::vector<ServerWindow*>* local_windows) {
  if (window->id().connection_id == id_) {
    if (local_windows)
      local_windows->push_back(GetWindow(window->id()));
    return;
  }
  known_windows_.erase(WindowIdToTransportId(window->id()));
  std::vector<const ServerWindow*> children = window->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    RemoveFromKnown(children[i], local_windows);
}

void WindowTreeImpl::RemoveRoot() {
  CHECK(root_.get());
  const WindowId root_id(*root_);
  root_.reset();
  // No need to do anything if we created the window.
  if (root_id.connection_id == id_)
    return;

  client()->OnUnembed();
  client()->OnWindowDeleted(WindowIdToTransportId(root_id));
  connection_manager_->OnConnectionMessagedClient(id_);

  // This connection no longer knows about the window. Unparent any windows that
  // were parented to windows in the root.
  std::vector<ServerWindow*> local_windows;
  RemoveFromKnown(GetWindow(root_id), &local_windows);
  for (size_t i = 0; i < local_windows.size(); ++i)
    local_windows[i]->parent()->Remove(local_windows[i]);
}

Array<mojom::WindowDataPtr> WindowTreeImpl::WindowsToWindowDatas(
    const std::vector<const ServerWindow*>& windows) {
  Array<mojom::WindowDataPtr> array(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    array[i] = WindowToWindowData(windows[i]).Pass();
  return array.Pass();
}

mojom::WindowDataPtr WindowTreeImpl::WindowToWindowData(
    const ServerWindow* window) {
  DCHECK(IsWindowKnown(window));
  const ServerWindow* parent = window->parent();
  // If the parent isn't known, it means the parent is not visible to us (not
  // in roots), and should not be sent over.
  if (parent && !IsWindowKnown(parent))
    parent = NULL;
  mojom::WindowDataPtr window_data(mojom::WindowData::New());
  window_data->parent_id =
      WindowIdToTransportId(parent ? parent->id() : WindowId());
  window_data->window_id = WindowIdToTransportId(window->id());
  window_data->bounds = Rect::From(window->bounds());
  window_data->properties =
      mojo::Map<String, Array<uint8_t>>::From(window->properties());
  window_data->visible = window->visible();
  window_data->drawn = window->IsDrawn();
  window_data->viewport_metrics =
      connection_manager_->GetViewportMetricsForWindow(window);
  return window_data.Pass();
}

void WindowTreeImpl::GetWindowTreeImpl(
    const ServerWindow* window,
    std::vector<const ServerWindow*>* windows) const {
  DCHECK(window);

  if (!access_policy_->CanGetWindowTree(window))
    return;

  windows->push_back(window);

  if (!access_policy_->CanDescendIntoWindowForWindowTree(window))
    return;

  std::vector<const ServerWindow*> children(window->GetChildren());
  for (size_t i = 0; i < children.size(); ++i)
    GetWindowTreeImpl(children[i], windows);
}

void WindowTreeImpl::NotifyDrawnStateChanged(const ServerWindow* window,
                                             bool new_drawn_value) {
  // Even though we don't know about window, it may be an ancestor of our root,
  // in
  // which case the change may effect our roots drawn state.
  if (!root_.get())
    return;

  const ServerWindow* root = GetWindow(*root_);
  DCHECK(root);
  if (window->Contains(root) && (new_drawn_value != root->IsDrawn())) {
    client()->OnWindowDrawnStateChanged(WindowIdToTransportId(root->id()),
                                        new_drawn_value);
  }
}

void WindowTreeImpl::DestroyWindows() {
  if (!window_map_.empty()) {
    ConnectionManager::ScopedChange change(this, connection_manager_, true);
    // If we get here from the destructor we're not going to get
    // ProcessWindowDeleted(). Copy the map and delete from the copy so that we
    // don't have to worry about whether |window_map_| changes or not.
    WindowMap window_map_copy;
    window_map_.swap(window_map_copy);
    STLDeleteValues(&window_map_copy);
  }
}

bool WindowTreeImpl::CanEmbed(const WindowId& window_id,
                              uint32_t policy_bitmask) const {
  const ServerWindow* window = GetWindow(window_id);
  return window && access_policy_->CanEmbed(window, policy_bitmask);
}

void WindowTreeImpl::PrepareForEmbed(const WindowId& window_id) {
  const ServerWindow* window = GetWindow(window_id);
  DCHECK(window);

  // Only allow a node to be the root for one connection.
  WindowTreeImpl* existing_owner =
      connection_manager_->GetConnectionWithRoot(window_id);

  ConnectionManager::ScopedChange change(this, connection_manager_, true);
  RemoveChildrenAsPartOfEmbed(window_id);
  if (existing_owner) {
    // Never message the originating connection.
    connection_manager_->OnConnectionMessagedClient(id_);
    existing_owner->RemoveRoot();
  }
}

void WindowTreeImpl::RemoveChildrenAsPartOfEmbed(const WindowId& window_id) {
  ServerWindow* window = GetWindow(window_id);
  CHECK(window);
  CHECK(window->id().connection_id == window_id.connection_id);
  std::vector<ServerWindow*> children = window->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    window->Remove(children[i]);
}

void WindowTreeImpl::NewWindow(
    Id transport_window_id,
    const Callback<void(mojom::ErrorCode)>& callback) {
  callback.Run(NewWindow(WindowIdFromTransportId(transport_window_id)));
}

void WindowTreeImpl::DeleteWindow(Id transport_window_id,
                                  const Callback<void(bool)>& callback) {
  ServerWindow* window =
      GetWindow(WindowIdFromTransportId(transport_window_id));
  bool success = false;
  if (window && access_policy_->CanDeleteWindow(window)) {
    WindowTreeImpl* connection =
        connection_manager_->GetConnection(window->id().connection_id);
    success = connection && connection->DeleteWindowImpl(this, window);
  }
  callback.Run(success);
}

void WindowTreeImpl::AddWindow(Id parent_id,
                               Id child_id,
                               const Callback<void(bool)>& callback) {
  callback.Run(AddWindow(WindowIdFromTransportId(parent_id),
                         WindowIdFromTransportId(child_id)));
}

void WindowTreeImpl::RemoveWindowFromParent(
    Id window_id,
    const Callback<void(bool)>& callback) {
  bool success = false;
  ServerWindow* window = GetWindow(WindowIdFromTransportId(window_id));
  if (window && window->parent() &&
      access_policy_->CanRemoveWindowFromParent(window)) {
    success = true;
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    window->parent()->Remove(window);
  }
  callback.Run(success);
}

void WindowTreeImpl::ReorderWindow(Id window_id,
                                   Id relative_window_id,
                                   mojom::OrderDirection direction,
                                   const Callback<void(bool)>& callback) {
  bool success = false;
  ServerWindow* window = GetWindow(WindowIdFromTransportId(window_id));
  ServerWindow* relative_window =
      GetWindow(WindowIdFromTransportId(relative_window_id));
  if (CanReorderWindow(window, relative_window, direction)) {
    success = true;
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    window->parent()->Reorder(window, relative_window, direction);
    connection_manager_->ProcessWindowReorder(window, relative_window,
                                              direction);
  }
  callback.Run(success);
}

void WindowTreeImpl::GetWindowTree(
    Id window_id,
    const Callback<void(Array<mojom::WindowDataPtr>)>& callback) {
  std::vector<const ServerWindow*> windows(
      GetWindowTree(WindowIdFromTransportId(window_id)));
  callback.Run(WindowsToWindowDatas(windows));
}

void WindowTreeImpl::SetWindowBounds(Id window_id,
                                     mojo::RectPtr bounds,
                                     const Callback<void(bool)>& callback) {
  ServerWindow* window = GetWindow(WindowIdFromTransportId(window_id));
  const bool success = window && access_policy_->CanSetWindowBounds(window);
  if (success) {
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    window->SetBounds(bounds.To<gfx::Rect>());
  }
  callback.Run(success);
}

void WindowTreeImpl::SetWindowVisibility(Id transport_window_id,
                                         bool visible,
                                         const Callback<void(bool)>& callback) {
  callback.Run(SetWindowVisibility(WindowIdFromTransportId(transport_window_id),
                                   visible));
}

void WindowTreeImpl::SetWindowProperty(
    uint32_t window_id,
    const mojo::String& name,
    mojo::Array<uint8_t> value,
    const mojo::Callback<void(bool)>& callback) {
  ServerWindow* window = GetWindow(WindowIdFromTransportId(window_id));
  const bool success = window && access_policy_->CanSetWindowProperties(window);
  if (success) {
    ConnectionManager::ScopedChange change(this, connection_manager_, false);

    if (value.is_null()) {
      window->SetProperty(name, nullptr);
    } else {
      std::vector<uint8_t> data = value.To<std::vector<uint8_t>>();
      window->SetProperty(name, &data);
    }
  }
  callback.Run(success);
}

void WindowTreeImpl::RequestSurface(
    Id window_id,
    mojo::InterfaceRequest<mojom::Surface> surface,
    mojom::SurfaceClientPtr client) {
  ServerWindow* window = GetWindow(WindowIdFromTransportId(window_id));
  const bool success = window && access_policy_->CanSetWindowSurfaceId(window);
  if (!success)
    return;
  window->Bind(surface.Pass(), client.Pass());
}

void WindowTreeImpl::SetWindowTextInputState(uint32_t window_id,
                                             mojo::TextInputStatePtr state) {
  ServerWindow* window = GetWindow(WindowIdFromTransportId(window_id));
  bool success = window && access_policy_->CanSetWindowTextInputState(window);
  if (success)
    window->SetTextInputState(state.To<ui::TextInputState>());
}

void WindowTreeImpl::SetImeVisibility(Id transport_window_id,
                                      bool visible,
                                      mojo::TextInputStatePtr state) {
  ServerWindow* window =
      GetWindow(WindowIdFromTransportId(transport_window_id));
  bool success = window && access_policy_->CanSetWindowTextInputState(window);
  if (success) {
    if (!state.is_null())
      window->SetTextInputState(state.To<ui::TextInputState>());

    WindowTreeHostImpl* host = GetHost();
    if (host)
      host->SetImeVisibility(window, visible);
  }
}

void WindowTreeImpl::SetClientArea(Id transport_window_id, mojo::RectPtr rect) {
  ServerWindow* window =
      GetWindow(WindowIdFromTransportId(transport_window_id));
  if (!window || !access_policy_->CanSetClientArea(window))
    return;

  if (rect.is_null())
    window->SetClientArea(gfx::Rect(window->bounds().size()));
  else
    window->SetClientArea(rect.To<gfx::Rect>());
}

void WindowTreeImpl::Embed(Id transport_window_id,
                           mojom::WindowTreeClientPtr client,
                           uint32_t policy_bitmask,
                           const EmbedCallback& callback) {
  ConnectionSpecificId connection_id = kInvalidConnectionId;
  const bool result = Embed(WindowIdFromTransportId(transport_window_id),
                            client.Pass(), policy_bitmask, &connection_id);
  callback.Run(result, connection_id);
}

void WindowTreeImpl::SetFocus(uint32_t window_id) {
  ServerWindow* window = GetWindow(WindowIdFromTransportId(window_id));
  // TODO(beng): consider shifting non-policy drawn check logic to VTH's
  //             FocusController.
  if (window && window->IsDrawn() && access_policy_->CanSetFocus(window)) {
    ConnectionManager::ScopedChange change(this, connection_manager_, false);
    WindowTreeHostImpl* host = GetHost();
    if (host)
      host->SetFocusedWindow(window);
  }
}

bool WindowTreeImpl::IsRootForAccessPolicy(const WindowId& id) const {
  return IsRoot(id);
}

bool WindowTreeImpl::IsWindowKnownForAccessPolicy(
    const ServerWindow* window) const {
  return IsWindowKnown(window);
}

bool WindowTreeImpl::IsWindowRootOfAnotherConnectionForAccessPolicy(
    const ServerWindow* window) const {
  WindowTreeImpl* connection =
      connection_manager_->GetConnectionWithRoot(window->id());
  return connection && connection != this;
}

bool WindowTreeImpl::IsDescendantOfEmbedRoot(const ServerWindow* window) {
  return is_embed_root_ && root_ && GetWindow(*root_)->Contains(window);
}

}  // namespace mus
