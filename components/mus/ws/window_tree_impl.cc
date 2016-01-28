// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/default_access_policy.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/operation.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_observer.h"
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
using mojo::String;

namespace mus {
namespace ws {

class TargetedEvent : public ServerWindowObserver {
 public:
  TargetedEvent(ServerWindow* target, mojom::EventPtr event)
      : target_(target), event_(std::move(event)) {
    target_->AddObserver(this);
  }
  ~TargetedEvent() override {
    if (target_)
      target_->RemoveObserver(this);
  }

  ServerWindow* target() { return target_; }
  mojom::EventPtr event() { return std::move(event_); }

 private:
  // ServerWindowObserver:
  void OnWindowDestroyed(ServerWindow* window) override {
    DCHECK_EQ(target_, window);
    target_->RemoveObserver(this);
    target_ = nullptr;
  }

  ServerWindow* target_;
  mojom::EventPtr event_;

  DISALLOW_COPY_AND_ASSIGN(TargetedEvent);
};

WindowTreeImpl::WindowTreeImpl(ConnectionManager* connection_manager,
                               ServerWindow* root,
                               uint32_t policy_bitmask)
    : connection_manager_(connection_manager),
      id_(connection_manager_->GetAndAdvanceNextConnectionId()),
      next_window_id_(1),
      client_(nullptr),
      event_ack_id_(0),
      event_source_host_(nullptr),
      is_embed_root_(false),
      window_manager_internal_(nullptr) {
  CHECK(root);
  roots_.insert(root);
  if (root->GetRoot() == root) {
    access_policy_.reset(new WindowManagerAccessPolicy(id_, this));
    is_embed_root_ = true;
  } else {
    access_policy_.reset(new DefaultAccessPolicy(id_, this));
    is_embed_root_ = (policy_bitmask & WindowTree::kAccessPolicyEmbedRoot) != 0;
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
  CHECK_EQ(1u, roots_.size());
  GetUnknownWindowsFrom(*roots_.begin(), &to_send);

  WindowTreeHostImpl* host = GetHost(*roots_.begin());
  const ServerWindow* focused_window =
      host ? host->GetFocusedWindow() : nullptr;
  if (focused_window)
    focused_window = access_policy_->GetWindowForFocusChange(focused_window);
  ClientWindowId focused_window_id;
  if (focused_window)
    IsWindowKnown(focused_window, &focused_window_id);

  client->OnEmbed(id_, WindowToWindowData(to_send.front()), std::move(tree),
                  focused_window_id.id,
                  is_embed_root_ ? WindowTree::kAccessPolicyEmbedRoot
                                 : WindowTree::kAccessPolicyDefault);
}

const ServerWindow* WindowTreeImpl::GetWindow(const WindowId& id) const {
  if (id_ == id.connection_id) {
    auto iter = created_window_map_.find(id);
    return iter == created_window_map_.end() ? nullptr : iter->second;
  }
  return connection_manager_->GetWindow(id);
}

bool WindowTreeImpl::IsWindowKnown(const ServerWindow* window,
                                   ClientWindowId* id) const {
  if (!window)
    return false;
  auto iter = window_id_to_client_id_map_.find(window->id());
  if (iter == window_id_to_client_id_map_.end())
    return false;
  if (id)
    *id = iter->second;
  return true;
}

bool WindowTreeImpl::HasRoot(const ServerWindow* window) const {
  return roots_.count(window) > 0;
}

const ServerWindow* WindowTreeImpl::GetWindowByClientId(
    const ClientWindowId& id) const {
  auto iter = client_id_to_window_id_map_.find(id);
  return iter == client_id_to_window_id_map_.end() ? nullptr
                                                   : GetWindow(iter->second);
}

const WindowTreeHostImpl* WindowTreeImpl::GetHost(
    const ServerWindow* window) const {
  return window ? connection_manager_->GetWindowTreeHostByWindow(window)
                : nullptr;
}

void WindowTreeImpl::OnWindowDestroyingTreeImpl(WindowTreeImpl* connection) {
  // Notify our client if |connection| was embedded in any of our views.
  for (const auto* connection_root : connection->roots_) {
    const bool owns_connection_root =
        connection_root->id().connection_id == id_;
    if (owns_connection_root ||
        (is_embed_root_ && IsWindowKnown(connection_root))) {
      client_->OnEmbeddedAppDisconnected(
          ClientWindowIdForWindow(connection_root).id);
    }
  }
}

void WindowTreeImpl::OnWillDestroyWindowTreeHost(
    WindowTreeHostImpl* tree_host) {
  if (event_source_host_ == tree_host)
    event_source_host_ = nullptr;
}

void WindowTreeImpl::NotifyChangeCompleted(
    uint32_t change_id,
    mojom::WindowManagerErrorCode error_code) {
  client_->OnChangeCompleted(
      change_id, error_code == mojom::WindowManagerErrorCode::SUCCESS);
}

bool WindowTreeImpl::NewWindow(
    const ClientWindowId& client_window_id,
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  if (!IsValidIdForNewWindow(client_window_id))
    return false;
  const WindowId window_id = GenerateNewWindowId();
  DCHECK(!GetWindow(window_id));
  ServerWindow* window =
      connection_manager_->CreateServerWindow(window_id, properties);
  created_window_map_[window_id] = window;
  client_id_to_window_id_map_[client_window_id] = window_id;
  window_id_to_client_id_map_[window_id] = client_window_id;
  return true;
}

bool WindowTreeImpl::AddWindow(const ClientWindowId& parent_id,
                               const ClientWindowId& child_id) {
  ServerWindow* parent = GetWindowByClientId(parent_id);
  ServerWindow* child = GetWindowByClientId(child_id);
  if (parent && child && child->parent() != parent &&
      !child->Contains(parent) && access_policy_->CanAddWindow(parent, child)) {
    Operation op(this, connection_manager_, OperationType::ADD_WINDOW);
    parent->Add(child);
    return true;
  }
  return false;
}

bool WindowTreeImpl::AddTransientWindow(
    const ClientWindowId& window_id,
    const ClientWindowId& transient_window_id) {
  ServerWindow* window = GetWindowByClientId(window_id);
  ServerWindow* transient_window = GetWindowByClientId(transient_window_id);
  if (window && transient_window && !transient_window->Contains(window) &&
      access_policy_->CanAddTransientWindow(window, transient_window)) {
    Operation op(this, connection_manager_,
                 OperationType::ADD_TRANSIENT_WINDOW);
    window->AddTransientWindow(transient_window);
    return true;
  }
  return false;
}

std::vector<const ServerWindow*> WindowTreeImpl::GetWindowTree(
    const ClientWindowId& window_id) const {
  const ServerWindow* window = GetWindowByClientId(window_id);
  std::vector<const ServerWindow*> windows;
  if (window)
    GetWindowTreeImpl(window, &windows);
  return windows;
}

bool WindowTreeImpl::SetWindowVisibility(const ClientWindowId& window_id,
                                         bool visible) {
  ServerWindow* window = GetWindowByClientId(window_id);
  if (!window || window->visible() == visible ||
      !access_policy_->CanChangeWindowVisibility(window)) {
    return false;
  }
  Operation op(this, connection_manager_, OperationType::SET_WINDOW_VISIBILITY);
  window->SetVisible(visible);
  return true;
}

bool WindowTreeImpl::Embed(const ClientWindowId& window_id,
                           mojom::WindowTreeClientPtr client,
                           uint32_t policy_bitmask,
                           ConnectionSpecificId* connection_id) {
  *connection_id = kInvalidConnectionId;
  if (!client || !CanEmbed(window_id, policy_bitmask))
    return false;
  ServerWindow* window = GetWindowByClientId(window_id);
  PrepareForEmbed(window);
  WindowTreeImpl* new_connection = connection_manager_->EmbedAtWindow(
      window, policy_bitmask, std::move(client));
  if (is_embed_root_)
    *connection_id = new_connection->id();
  return true;
}

void WindowTreeImpl::DispatchInputEvent(ServerWindow* target,
                                        mojom::EventPtr event) {
  if (event_ack_id_) {
    // This is currently waiting for an event ack. Add it to the queue.
    event_queue_.push(
        make_scoped_ptr(new TargetedEvent(target, std::move(event))));
    // TODO(sad): If the |event_queue_| grows too large, then this should notify
    // WindowTreeHostImpl, so that it can stop sending events.
    return;
  }

  // If there are events in the queue, then store this new event in the queue,
  // and dispatch the latest event from the queue instead that still has a live
  // target.
  if (!event_queue_.empty()) {
    event_queue_.push(
        make_scoped_ptr(new TargetedEvent(target, std::move(event))));
    return;
  }

  DispatchInputEventImpl(target, std::move(event));
}

bool WindowTreeImpl::IsWaitingForNewTopLevelWindow(uint32_t wm_change_id) {
  return waiting_for_top_level_window_info_ &&
         waiting_for_top_level_window_info_->wm_change_id == wm_change_id;
}

void WindowTreeImpl::OnWindowManagerCreatedTopLevelWindow(
    uint32_t wm_change_id,
    uint32_t client_change_id,
    const ServerWindow* window) {
  DCHECK(IsWaitingForNewTopLevelWindow(wm_change_id));
  scoped_ptr<WaitingForTopLevelWindowInfo> waiting_for_top_level_window_info(
      std::move(waiting_for_top_level_window_info_));
  connection_manager_->GetClientConnection(this)
      ->SetIncomingMethodCallProcessingPaused(false);
  // We were paused, so the id should still be valid.
  DCHECK(IsValidIdForNewWindow(
      waiting_for_top_level_window_info->client_window_id));
  client_id_to_window_id_map_[waiting_for_top_level_window_info
                                  ->client_window_id] = window->id();
  window_id_to_client_id_map_[window->id()] =
      waiting_for_top_level_window_info->client_window_id;
  roots_.insert(window);
  client_->OnTopLevelCreated(client_change_id, WindowToWindowData(window));
}

void WindowTreeImpl::OnChangeCompleted(uint32_t change_id, bool success) {
  client_->OnChangeCompleted(change_id, success);
}

void WindowTreeImpl::ProcessWindowBoundsChanged(const ServerWindow* window,
                                                const gfx::Rect& old_bounds,
                                                const gfx::Rect& new_bounds,
                                                bool originated_change) {
  ClientWindowId client_window_id;
  if (originated_change || !IsWindowKnown(window, &client_window_id))
    return;
  client_->OnWindowBoundsChanged(client_window_id.id, Rect::From(old_bounds),
                                 Rect::From(new_bounds));
}

void WindowTreeImpl::ProcessClientAreaChanged(
    const ServerWindow* window,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas,
    bool originated_change) {
  ClientWindowId client_window_id;
  if (originated_change || !IsWindowKnown(window, &client_window_id))
    return;
  client_->OnClientAreaChanged(
      client_window_id.id, mojo::Insets::From(new_client_area),
      mojo::Array<mojo::RectPtr>::From(new_additional_client_areas));
}

void WindowTreeImpl::ProcessViewportMetricsChanged(
    WindowTreeHostImpl* host,
    const mojom::ViewportMetrics& old_metrics,
    const mojom::ViewportMetrics& new_metrics,
    bool originated_change) {
  mojo::Array<Id> window_ids;
  for (const ServerWindow* root : roots_) {
    if (GetHost(root) == host) {
      ClientWindowId client_window_id;
      const bool known = IsWindowKnown(root, &client_window_id);
      DCHECK(known);
      window_ids.push_back(client_window_id.id);
    }
  }
  if (window_ids.size() == 0u)
    return;

  client_->OnWindowViewportMetricsChanged(
      std::move(window_ids), old_metrics.Clone(), new_metrics.Clone());
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

  ClientWindowId client_window_id;
  if (!IsWindowKnown(window, &client_window_id))
    return;

  Array<uint8_t> data;
  if (new_data)
    data = Array<uint8_t>::From(*new_data);

  client_->OnWindowSharedPropertyChanged(client_window_id.id, String(name),
                                         std::move(data));
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
  if (originated_change || (connection_manager_->current_operation_type() ==
                            OperationType::DELETE_WINDOW) ||
      (connection_manager_->current_operation_type() == OperationType::EMBED) ||
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
  const ClientWindowId new_parent_client_window_id =
      new_parent ? ClientWindowIdForWindow(new_parent) : ClientWindowId();
  const ClientWindowId old_parent_client_window_id =
      old_parent ? ClientWindowIdForWindow(old_parent) : ClientWindowId();
  const ClientWindowId client_window_id =
      window ? ClientWindowIdForWindow(window) : ClientWindowId();
  client_->OnWindowHierarchyChanged(
      client_window_id.id, new_parent_client_window_id.id,
      old_parent_client_window_id.id, WindowsToWindowDatas(to_send));
  connection_manager_->OnConnectionMessagedClient(id_);
}

void WindowTreeImpl::ProcessWindowReorder(const ServerWindow* window,
                                          const ServerWindow* relative_window,
                                          mojom::OrderDirection direction,
                                          bool originated_change) {
  DCHECK_EQ(window->parent(), relative_window->parent());
  ClientWindowId client_window_id, relative_client_window_id;
  if (originated_change || !IsWindowKnown(window, &client_window_id) ||
      !IsWindowKnown(relative_window, &relative_client_window_id) ||
      connection_manager_->DidConnectionMessageClient(id_))
    return;

  // Do not notify ordering changes of the root windows, since the client
  // doesn't know about the ancestors of the roots, and so can't do anything
  // about this ordering change of the root.
  if (HasRoot(window) || HasRoot(relative_window))
    return;

  client_->OnWindowReordered(client_window_id.id, relative_client_window_id.id,
                             direction);
  connection_manager_->OnConnectionMessagedClient(id_);
}

void WindowTreeImpl::ProcessWindowDeleted(const ServerWindow* window,
                                          bool originated_change) {
  if (window->id().connection_id == id_)
    created_window_map_.erase(window->id());

  ClientWindowId client_window_id;
  if (!IsWindowKnown(window, &client_window_id))
    return;

  if (HasRoot(window))
    RemoveRoot(window, RemoveRootReason::DELETED);
  else
    RemoveFromMaps(window);

  if (originated_change)
    return;

  client_->OnWindowDeleted(client_window_id.id);
  connection_manager_->OnConnectionMessagedClient(id_);
}

void WindowTreeImpl::ProcessWillChangeWindowVisibility(
    const ServerWindow* window,
    bool originated_change) {
  if (originated_change)
    return;

  ClientWindowId client_window_id;
  if (IsWindowKnown(window, &client_window_id)) {
    client_->OnWindowVisibilityChanged(client_window_id.id, !window->visible());
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

void WindowTreeImpl::ProcessCursorChanged(const ServerWindow* window,
                                          int32_t cursor_id,
                                          bool originated_change) {
  if (originated_change)
    return;
  ClientWindowId client_window_id;
  if (!IsWindowKnown(window, &client_window_id))
    return;

  client_->OnWindowPredefinedCursorChanged(client_window_id.id,
                                           mojom::Cursor(cursor_id));
}

void WindowTreeImpl::ProcessFocusChanged(
    const ServerWindow* old_focused_window,
    const ServerWindow* new_focused_window) {
  const ServerWindow* window =
      new_focused_window
          ? access_policy_->GetWindowForFocusChange(new_focused_window)
          : nullptr;
  ClientWindowId client_window_id;
  // If the window isn't known we'll supply null, which is ok.
  IsWindowKnown(window, &client_window_id);
  // TODO(sky): this should only notify if this results in a change of focus
  // for the client.
  client_->OnWindowFocused(client_window_id.id);
}

void WindowTreeImpl::ProcessTransientWindowAdded(
    const ServerWindow* window,
    const ServerWindow* transient_window,
    bool originated_change) {
  if (originated_change)
    return;

  ClientWindowId client_window_id, transient_client_window_id;
  if (!IsWindowKnown(window, &client_window_id) ||
      !IsWindowKnown(transient_window, &transient_client_window_id)) {
    return;
  }
  client_->OnTransientWindowAdded(client_window_id.id,
                                  transient_client_window_id.id);
}

void WindowTreeImpl::ProcessTransientWindowRemoved(
    const ServerWindow* window,
    const ServerWindow* transient_window,
    bool originated_change) {
  if (originated_change)
    return;
  ClientWindowId client_window_id, transient_client_window_id;
  if (!IsWindowKnown(window, &client_window_id) ||
      !IsWindowKnown(transient_window, &transient_client_window_id)) {
    return;
  }
  client_->OnTransientWindowRemoved(client_window_id.id,
                                    transient_client_window_id.id);
}

WindowTreeHostImpl* WindowTreeImpl::GetHostForWindowManager() {
  // The WindowTreeImpl for the wm has one and only one root.
  CHECK_EQ(1u, roots_.size());

  // Indicates this connection is for the wm.
  DCHECK(window_manager_internal_);

  WindowTreeHostImpl* host = GetHost(*roots_.begin());
  CHECK(host);
  DCHECK_EQ(this, host->GetWindowTree());
  return host;
}

bool WindowTreeImpl::ShouldRouteToWindowManager(
    const ServerWindow* window) const {
  // If the client created this window, then do not route it through the WM.
  if (window->id().connection_id == id_)
    return false;

  // If the client did not create the window, then it must be the root of the
  // client. If not, that means the client should not know about this window,
  // and so do not route the request to the WM.
  if (roots_.count(window) == 0)
    return false;

  // The WindowManager is attached to the root of the WindowTreeHost, if there
  // isn't a WindowManager attached no need to route to it.
  const WindowTreeHostImpl* host = GetHost(window);
  if (!host || !host->GetWindowTree() ||
      !host->GetWindowTree()->window_manager_internal_) {
    return false;
  }

  // Requests coming from the WM should not be routed through the WM again.
  const bool is_wm = host->GetWindowTree() == this;
  return is_wm ? false : true;
}

ClientWindowId WindowTreeImpl::ClientWindowIdForWindow(
    const ServerWindow* window) const {
  auto iter = window_id_to_client_id_map_.find(window->id());
  DCHECK(iter != window_id_to_client_id_map_.end());
  return iter->second;
}

bool WindowTreeImpl::IsValidIdForNewWindow(const ClientWindowId& id) const {
  if (is_embed_root_ && WindowIdFromTransportId(id.id).connection_id != id_) {
    // Embed roots see windows created from other connections. If they don't
    // use the connection id when creating windows the client could end up with
    // two windows with the same id. Because of this we restrict the ids such
    // connections can create.
    return false;
  }
  return client_id_to_window_id_map_.count(id) == 0u;
}

WindowId WindowTreeImpl::GenerateNewWindowId() {
  // TODO(sky): deal with wrapping and uniqueness.
  return WindowId(id_, next_window_id_++);
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
  if ((direction == mojom::OrderDirection::ABOVE && child_i == target_i + 1) ||
      (direction == mojom::OrderDirection::BELOW && child_i + 1 == target_i)) {
    return false;
  }

  return true;
}

bool WindowTreeImpl::DeleteWindowImpl(WindowTreeImpl* source,
                                      ServerWindow* window) {
  DCHECK(window);
  DCHECK_EQ(window->id().connection_id, id_);
  Operation op(source, connection_manager_, OperationType::DELETE_WINDOW);
  delete window;
  return true;
}

void WindowTreeImpl::GetUnknownWindowsFrom(
    const ServerWindow* window,
    std::vector<const ServerWindow*>* windows) {
  if (IsWindowKnown(window) || !access_policy_->CanGetWindowTree(window))
    return;
  windows->push_back(window);
  // There are two cases where this gets hit:
  // . During init, in which case using the window id as the client id is
  //   fine.
  // . When a window is moved to a parent of a window we know about. This is
  //   only encountered for the WM or embed roots. We assume such clients want
  //   to see the real id of the window and are only created ClientWindowIds
  //   with the connection_id.
  const ClientWindowId client_window_id(WindowIdToTransportId(window->id()));
  DCHECK_EQ(0u, client_id_to_window_id_map_.count(client_window_id));
  client_id_to_window_id_map_[client_window_id] = window->id();
  window_id_to_client_id_map_[window->id()] = client_window_id;
  if (!access_policy_->CanDescendIntoWindowForWindowTree(window))
    return;
  std::vector<const ServerWindow*> children(window->GetChildren());
  for (size_t i = 0; i < children.size(); ++i)
    GetUnknownWindowsFrom(children[i], windows);
}

bool WindowTreeImpl::RemoveFromMaps(const ServerWindow* window) {
  auto iter = window_id_to_client_id_map_.find(window->id());
  if (iter == window_id_to_client_id_map_.end())
    return false;

  client_id_to_window_id_map_.erase(iter->second);
  window_id_to_client_id_map_.erase(iter);
  return true;
}

void WindowTreeImpl::RemoveFromKnown(
    const ServerWindow* window,
    std::vector<ServerWindow*>* local_windows) {
  if (window->id().connection_id == id_) {
    if (local_windows)
      local_windows->push_back(GetWindow(window->id()));
    return;
  }

  RemoveFromMaps(window);

  std::vector<const ServerWindow*> children = window->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    RemoveFromKnown(children[i], local_windows);
}

void WindowTreeImpl::RemoveRoot(const ServerWindow* window,
                                RemoveRootReason reason) {
  DCHECK(roots_.count(window) > 0);
  roots_.erase(window);

  const ClientWindowId client_window_id(ClientWindowIdForWindow(window));

  // No need to do anything if we created the window.
  if (window->id().connection_id == id_)
    return;

  if (reason == RemoveRootReason::EMBED) {
    client_->OnUnembed(client_window_id.id);
    client_->OnWindowDeleted(client_window_id.id);
    connection_manager_->OnConnectionMessagedClient(id_);
  }

  // This connection no longer knows about the window. Unparent any windows that
  // were parented to windows in the root.
  std::vector<ServerWindow*> local_windows;
  RemoveFromKnown(window, &local_windows);
  for (size_t i = 0; i < local_windows.size(); ++i)
    local_windows[i]->parent()->Remove(local_windows[i]);
}

Array<mojom::WindowDataPtr> WindowTreeImpl::WindowsToWindowDatas(
    const std::vector<const ServerWindow*>& windows) {
  Array<mojom::WindowDataPtr> array(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    array[i] = WindowToWindowData(windows[i]);
  return array;
}

mojom::WindowDataPtr WindowTreeImpl::WindowToWindowData(
    const ServerWindow* window) {
  DCHECK(IsWindowKnown(window));
  const ServerWindow* parent = window->parent();
  // If the parent isn't known, it means the parent is not visible to us (not
  // in roots), and should not be sent over.
  if (parent && !IsWindowKnown(parent))
    parent = nullptr;
  mojom::WindowDataPtr window_data(mojom::WindowData::New());
  window_data->parent_id =
      parent ? ClientWindowIdForWindow(parent).id : ClientWindowId().id;
  window_data->window_id =
      window ? ClientWindowIdForWindow(window).id : ClientWindowId().id;
  window_data->bounds = Rect::From(window->bounds());
  window_data->properties =
      mojo::Map<String, Array<uint8_t>>::From(window->properties());
  window_data->visible = window->visible();
  window_data->drawn = window->IsDrawn();
  window_data->viewport_metrics =
      connection_manager_->GetViewportMetricsForWindow(window);
  return window_data;
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
  // in which case the change may effect our roots drawn state.
  if (roots_.empty())
    return;

  for (auto* root : roots_) {
    if (window->Contains(root) && (new_drawn_value != root->IsDrawn())) {
      client_->OnWindowDrawnStateChanged(ClientWindowIdForWindow(root).id,
                                         new_drawn_value);
    }
  }
}

void WindowTreeImpl::DestroyWindows() {
  if (created_window_map_.empty())
    return;

  Operation op(this, connection_manager_, OperationType::DELETE_WINDOW);
  // If we get here from the destructor we're not going to get
  // ProcessWindowDeleted(). Copy the map and delete from the copy so that we
  // don't have to worry about whether |created_window_map_| changes or not.
  base::hash_map<WindowId, ServerWindow*> created_window_map_copy;
  created_window_map_.swap(created_window_map_copy);
  // A sibling can be a transient parent of another window so we detach windows
  // from their transient parents to avoid double deletes.
  for (auto& pair : created_window_map_copy) {
    ServerWindow* transient_parent = pair.second->transient_parent();
    if (transient_parent)
      transient_parent->RemoveTransientWindow(pair.second);
  }
  STLDeleteValues(&created_window_map_copy);
}

bool WindowTreeImpl::CanEmbed(const ClientWindowId& window_id,
                              uint32_t policy_bitmask) const {
  const ServerWindow* window = GetWindowByClientId(window_id);
  return window && access_policy_->CanEmbed(window, policy_bitmask);
}

void WindowTreeImpl::PrepareForEmbed(ServerWindow* window) {
  DCHECK(window);

  // Only allow a node to be the root for one connection.
  WindowTreeImpl* existing_owner =
      connection_manager_->GetConnectionWithRoot(window);

  Operation op(this, connection_manager_, OperationType::EMBED);
  RemoveChildrenAsPartOfEmbed(window);
  if (existing_owner) {
    // Never message the originating connection.
    connection_manager_->OnConnectionMessagedClient(id_);
    existing_owner->RemoveRoot(window, RemoveRootReason::EMBED);
  }
}

void WindowTreeImpl::RemoveChildrenAsPartOfEmbed(ServerWindow* window) {
  CHECK(window);
  std::vector<ServerWindow*> children = window->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    window->Remove(children[i]);
}

void WindowTreeImpl::DispatchInputEventImpl(ServerWindow* target,
                                            mojom::EventPtr event) {
  DCHECK(!event_ack_id_);
  // We do not want to create a sequential id for each event, because that can
  // leak some information to the client. So instead, manufacture the id from
  // the event pointer.
  event_ack_id_ =
      0x1000000 | (reinterpret_cast<uintptr_t>(event.get()) & 0xffffff);
  event_source_host_ = GetHost(target);
  // Should only get events from windows attached to a host.
  DCHECK(event_source_host_);
  client_->OnWindowInputEvent(event_ack_id_, ClientWindowIdForWindow(target).id,
                              std::move(event));
}

void WindowTreeImpl::NewWindow(
    uint32_t change_id,
    Id transport_window_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties) {
  std::map<std::string, std::vector<uint8_t>> properties;
  if (!transport_properties.is_null()) {
    properties =
        transport_properties.To<std::map<std::string, std::vector<uint8_t>>>();
  }
  client_->OnChangeCompleted(
      change_id, NewWindow(ClientWindowId(transport_window_id), properties));
}

void WindowTreeImpl::NewTopLevelWindow(
    uint32_t change_id,
    Id transport_window_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties) {
  DCHECK(!waiting_for_top_level_window_info_);
  const ClientWindowId client_window_id(transport_window_id);
  // TODO(sky): need a way for client to provide context.
  WindowTreeHostImpl* tree_host =
      connection_manager_->GetActiveWindowTreeHost();
  if (!tree_host || tree_host->GetWindowTree() == this ||
      !IsValidIdForNewWindow(client_window_id)) {
    client_->OnChangeCompleted(change_id, false);
    return;
  }

  // The server creates the real window. Any further messages from the client
  // may try to alter the window. Pause incoming messages so that we know we
  // can't get a message for a window before the window is created. Once the
  // window is created we'll resume processing.
  connection_manager_->GetClientConnection(this)
      ->SetIncomingMethodCallProcessingPaused(true);

  const uint32_t wm_change_id =
      connection_manager_->GenerateWindowManagerChangeId(this, change_id);

  waiting_for_top_level_window_info_.reset(
      new WaitingForTopLevelWindowInfo(client_window_id, wm_change_id));

  tree_host->GetWindowTree()->window_manager_internal_->WmCreateTopLevelWindow(
      wm_change_id, std::move(transport_properties));
}

void WindowTreeImpl::DeleteWindow(uint32_t change_id, Id transport_window_id) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  bool success = false;
  bool should_close = window && (access_policy_->CanDeleteWindow(window) ||
                                 ShouldRouteToWindowManager(window));
  if (should_close) {
    WindowTreeImpl* connection =
        connection_manager_->GetConnection(window->id().connection_id);
    success = connection && connection->DeleteWindowImpl(this, window);
  }
  client_->OnChangeCompleted(change_id, success);
}

void WindowTreeImpl::AddWindow(uint32_t change_id, Id parent_id, Id child_id) {
  client_->OnChangeCompleted(change_id, AddWindow(ClientWindowId(parent_id),
                                                  ClientWindowId(child_id)));
}

void WindowTreeImpl::RemoveWindowFromParent(uint32_t change_id, Id window_id) {
  bool success = false;
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  if (window && window->parent() &&
      access_policy_->CanRemoveWindowFromParent(window)) {
    success = true;
    Operation op(this, connection_manager_,
                 OperationType::REMOVE_WINDOW_FROM_PARENT);
    window->parent()->Remove(window);
  }
  client_->OnChangeCompleted(change_id, success);
}

void WindowTreeImpl::AddTransientWindow(uint32_t change_id,
                                        Id window,
                                        Id transient_window) {
  client_->OnChangeCompleted(
      change_id, AddTransientWindow(ClientWindowId(window),
                                    ClientWindowId(transient_window)));
}

void WindowTreeImpl::RemoveTransientWindowFromParent(uint32_t change_id,
                                                     Id transient_window_id) {
  bool success = false;
  ServerWindow* transient_window =
      GetWindowByClientId(ClientWindowId(transient_window_id));
  if (transient_window && transient_window->transient_parent() &&
      access_policy_->CanRemoveTransientWindowFromParent(transient_window)) {
    success = true;
    Operation op(this, connection_manager_,
                 OperationType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT);
    transient_window->transient_parent()->RemoveTransientWindow(
        transient_window);
  }
  client_->OnChangeCompleted(change_id, success);
}

void WindowTreeImpl::ReorderWindow(uint32_t change_id,
                                   Id window_id,
                                   Id relative_window_id,
                                   mojom::OrderDirection direction) {
  bool success = false;
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  ServerWindow* relative_window =
      GetWindowByClientId(ClientWindowId(relative_window_id));
  if (CanReorderWindow(window, relative_window, direction)) {
    success = true;
    Operation op(this, connection_manager_, OperationType::REORDER_WINDOW);
    window->Reorder(relative_window, direction);
    connection_manager_->ProcessWindowReorder(window, relative_window,
                                              direction);
  }
  client_->OnChangeCompleted(change_id, success);
}

void WindowTreeImpl::GetWindowTree(
    Id window_id,
    const Callback<void(Array<mojom::WindowDataPtr>)>& callback) {
  std::vector<const ServerWindow*> windows(
      GetWindowTree(ClientWindowId(window_id)));
  callback.Run(WindowsToWindowDatas(windows));
}

void WindowTreeImpl::SetWindowBounds(uint32_t change_id,
                                     Id window_id,
                                     mojo::RectPtr bounds) {
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  if (window && ShouldRouteToWindowManager(window)) {
    const uint32_t wm_change_id =
        connection_manager_->GenerateWindowManagerChangeId(this, change_id);
    // |window_id| may be a client id, use the id from the window to ensure
    // the windowmanager doesn't get an id it doesn't know about.
    WindowTreeImpl* wm_window_tree = GetHost(window)->GetWindowTree();
    wm_window_tree->window_manager_internal_->WmSetBounds(
        wm_change_id, wm_window_tree->ClientWindowIdForWindow(window).id,
        std::move(bounds));
    return;
  }

  // Only the owner of the window can change the bounds.
  bool success = window && access_policy_->CanSetWindowBounds(window);
  if (success) {
    Operation op(this, connection_manager_, OperationType::SET_WINDOW_BOUNDS);
    window->SetBounds(bounds.To<gfx::Rect>());
  }
  client_->OnChangeCompleted(change_id, success);
}

void WindowTreeImpl::SetWindowVisibility(uint32_t change_id,
                                         Id transport_window_id,
                                         bool visible) {
  client_->OnChangeCompleted(
      change_id,
      SetWindowVisibility(ClientWindowId(transport_window_id), visible));
}

void WindowTreeImpl::SetWindowProperty(uint32_t change_id,
                                       Id transport_window_id,
                                       const mojo::String& name,
                                       mojo::Array<uint8_t> value) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  if (window && ShouldRouteToWindowManager(window)) {
    const uint32_t wm_change_id =
        connection_manager_->GenerateWindowManagerChangeId(this, change_id);
    WindowTreeImpl* wm_window_tree = GetHost(window)->GetWindowTree();
    wm_window_tree->window_manager_internal_->WmSetProperty(
        wm_change_id, wm_window_tree->ClientWindowIdForWindow(window).id, name,
        std::move(value));
    return;
  }
  const bool success = window && access_policy_->CanSetWindowProperties(window);
  if (success) {
    Operation op(this, connection_manager_, OperationType::SET_WINDOW_PROPERTY);
    if (value.is_null()) {
      window->SetProperty(name, nullptr);
    } else {
      std::vector<uint8_t> data = value.To<std::vector<uint8_t>>();
      window->SetProperty(name, &data);
    }
  }
  client_->OnChangeCompleted(change_id, success);
}

void WindowTreeImpl::AttachSurface(
    Id transport_window_id,
    mojom::SurfaceType type,
    mojo::InterfaceRequest<mojom::Surface> surface,
    mojom::SurfaceClientPtr client) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  const bool success =
      window && access_policy_->CanSetWindowSurface(window, type);
  if (!success)
    return;
  window->CreateSurface(type, std::move(surface), std::move(client));
}

void WindowTreeImpl::SetWindowTextInputState(Id transport_window_id,
                                             mojo::TextInputStatePtr state) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  bool success = window && access_policy_->CanSetWindowTextInputState(window);
  if (success)
    window->SetTextInputState(state.To<ui::TextInputState>());
}

void WindowTreeImpl::SetImeVisibility(Id transport_window_id,
                                      bool visible,
                                      mojo::TextInputStatePtr state) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  bool success = window && access_policy_->CanSetWindowTextInputState(window);
  if (success) {
    if (!state.is_null())
      window->SetTextInputState(state.To<ui::TextInputState>());

    WindowTreeHostImpl* host = GetHost(window);
    if (host)
      host->SetImeVisibility(window, visible);
  }
}

void WindowTreeImpl::OnWindowInputEventAck(uint32_t event_id) {
  if (event_ack_id_ == 0 || event_id != event_ack_id_) {
    // TODO(sad): Something bad happened. Kill the client?
    NOTIMPLEMENTED() << "Wrong event acked.";
  }
  event_ack_id_ = 0;

  WindowTreeHostImpl* event_source_host = event_source_host_;
  event_source_host_ = nullptr;
  if (event_source_host)
    event_source_host->OnEventAck(this);

  if (!event_queue_.empty()) {
    DCHECK(!event_ack_id_);
    ServerWindow* target = nullptr;
    mojom::EventPtr event;
    do {
      scoped_ptr<TargetedEvent> targeted_event =
          std::move(event_queue_.front());
      event_queue_.pop();
      target = targeted_event->target();
      event = targeted_event->event();
    } while (!event_queue_.empty() && !GetHost(target));
    if (target)
      DispatchInputEventImpl(target, std::move(event));
  }
}

void WindowTreeImpl::SetClientArea(
    Id transport_window_id,
    mojo::InsetsPtr insets,
    mojo::Array<mojo::RectPtr> transport_additional_client_areas) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  if (!window || !access_policy_->CanSetClientArea(window))
    return;

  std::vector<gfx::Rect> additional_client_areas =
      transport_additional_client_areas.To<std::vector<gfx::Rect>>();
  window->SetClientArea(insets.To<gfx::Insets>(), additional_client_areas);
}

void WindowTreeImpl::Embed(Id transport_window_id,
                           mojom::WindowTreeClientPtr client,
                           uint32_t policy_bitmask,
                           const EmbedCallback& callback) {
  ConnectionSpecificId connection_id = kInvalidConnectionId;
  const bool result = Embed(ClientWindowId(transport_window_id),
                            std::move(client), policy_bitmask, &connection_id);
  callback.Run(result, connection_id);
}

void WindowTreeImpl::SetFocus(uint32_t change_id, Id transport_window_id) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  // TODO(beng): consider shifting non-policy drawn check logic to VTH's
  //             FocusController.
  // TODO(sky): this doesn't work to clear focus. That is because if window is
  // null, then |host| is null and we fail.
  WindowTreeHostImpl* host = GetHost(window);
  const bool success = window && window->IsDrawn() && window->can_focus() &&
                       access_policy_->CanSetFocus(window) && host;
  if (success) {
    Operation op(this, connection_manager_, OperationType::SET_FOCUS);
    host->SetFocusedWindow(window);
  }
  client_->OnChangeCompleted(change_id, success);
}

void WindowTreeImpl::SetCanFocus(Id transport_window_id, bool can_focus) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  // TODO(sky): there should be an else case (it shouldn't route to wm and
  // policy allows, then set_can_focus).
  if (window && ShouldRouteToWindowManager(window))
    window->set_can_focus(can_focus);
}

void WindowTreeImpl::SetPredefinedCursor(uint32_t change_id,
                                         Id transport_window_id,
                                         mus::mojom::Cursor cursor_id) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));

  // Only the owner of the window can change the bounds.
  bool success = window && access_policy_->CanSetCursorProperties(window);
  if (success) {
    Operation op(this, connection_manager_,
                 OperationType::SET_WINDOW_PREDEFINED_CURSOR);
    window->SetPredefinedCursor(cursor_id);
  }
  client_->OnChangeCompleted(change_id, success);
}

void WindowTreeImpl::GetWindowManagerInternalClient(
    mojo::AssociatedInterfaceRequest<mojom::WindowManagerInternalClient>
        internal) {
  if (!access_policy_->CanSetWindowManagerInternal() ||
      window_manager_internal_)
    return;
  window_manager_internal_client_binding_.reset(
      new mojo::AssociatedBinding<mojom::WindowManagerInternalClient>(
          this, std::move(internal)));

  window_manager_internal_ = connection_manager_->GetClientConnection(this)
                                 ->GetWindowManagerInternal();
}

void WindowTreeImpl::WmResponse(uint32_t change_id, bool response) {
  // TODO(sky): think about what else case means.
  if (GetHostForWindowManager())
    connection_manager_->WindowManagerChangeCompleted(change_id, response);
}

void WindowTreeImpl::WmRequestClose(Id transport_window_id) {
  // Only the WindowManager should be using this.
  WindowTreeHostImpl* host = GetHostForWindowManager();
  if (!host)
    return;

  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  WindowTreeImpl* connection =
      connection_manager_->GetConnectionWithRoot(window);
  if (connection && connection != host->GetWindowTree())
    connection->client_->RequestClose(
        connection->ClientWindowIdForWindow(window).id);
  // TODO(sky): think about what else case means.
}

void WindowTreeImpl::WmSetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  if (GetHostForWindowManager())
    GetHostForWindowManager()->SetFrameDecorationValues(std::move(values));
}

void WindowTreeImpl::OnWmCreatedTopLevelWindow(uint32_t change_id,
                                               Id transport_window_id) {
  if (GetHostForWindowManager()) {
    ServerWindow* window =
        GetWindowByClientId(ClientWindowId(transport_window_id));
    if (window && window->id().connection_id != id_) {
      connection_manager_->WindowManagerSentBogusMessage();
      window = nullptr;
    }
    connection_manager_->WindowManagerCreatedTopLevelWindow(this, change_id,
                                                            window);
  }
  // TODO(sky): think about what else case means.
}

bool WindowTreeImpl::HasRootForAccessPolicy(const ServerWindow* window) const {
  return HasRoot(window);
}

bool WindowTreeImpl::IsWindowKnownForAccessPolicy(
    const ServerWindow* window) const {
  return IsWindowKnown(window);
}

bool WindowTreeImpl::IsWindowRootOfAnotherConnectionForAccessPolicy(
    const ServerWindow* window) const {
  WindowTreeImpl* connection =
      connection_manager_->GetConnectionWithRoot(window);
  return connection && connection != this;
}

bool WindowTreeImpl::IsDescendantOfEmbedRoot(const ServerWindow* window) {
  if (!is_embed_root_)
    return false;

  for (const auto* root : roots_) {
    if (root->Contains(window))
      return true;
  }
  return false;
}

}  // namespace ws
}  // namespace mus
