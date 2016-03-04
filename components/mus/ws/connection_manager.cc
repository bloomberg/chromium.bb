// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/connection_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/operation.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "components/mus/ws/window_manager_factory_service.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_tree_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/shell/public/cpp/connection.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace mus {
namespace ws {

ConnectionManager::ConnectionManager(
    ConnectionManagerDelegate* delegate,
    const scoped_refptr<mus::SurfacesState>& surfaces_state)
    : delegate_(delegate),
      surfaces_state_(surfaces_state),
      next_connection_id_(1),
      next_root_id_(0),
      current_operation_(nullptr),
      in_destructor_(false),
      next_wm_change_id_(0),
      got_valid_frame_decorations_(false),
      window_manager_factory_registry_(this, &user_id_tracker_) {}

ConnectionManager::~ConnectionManager() {
  in_destructor_ = true;

  while (!pending_displays_.empty())
    DestroyDisplay(*pending_displays_.begin());
  DCHECK(pending_displays_.empty());

  // DestroyDisplay() removes from |displays_| and deletes the Display.
  while (!displays_.empty())
    DestroyDisplay(*displays_.begin());
  DCHECK(displays_.empty());

  tree_map_.clear();
}

void ConnectionManager::AddDisplay(Display* display) {
  DCHECK_EQ(0u, pending_displays_.count(display));
  pending_displays_.insert(display);
}

void ConnectionManager::DestroyDisplay(Display* display) {
  for (auto& pair : tree_map_)
    pair.second->OnWillDestroyDisplay(display);

  if (pending_displays_.count(display)) {
    pending_displays_.erase(display);
  } else {
    DCHECK(displays_.count(display));
    displays_.erase(display);
  }
  delete display;

  // If we have no more roots left, let the app know so it can terminate.
  if (!displays_.size() && !pending_displays_.size())
    delegate_->OnNoMoreRootConnections();
}

ServerWindow* ConnectionManager::CreateServerWindow(
    const WindowId& id,
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  ServerWindow* window = new ServerWindow(this, id, properties);
  window->AddObserver(this);
  return window;
}

ConnectionSpecificId ConnectionManager::GetAndAdvanceNextConnectionId() {
  const ConnectionSpecificId id = next_connection_id_++;
  DCHECK_LT(id, next_connection_id_);
  return id;
}

uint16_t ConnectionManager::GetAndAdvanceNextRootId() {
  // TODO(sky): handle wrapping!
  const uint16_t id = next_root_id_++;
  DCHECK_LT(id, next_root_id_);
  return id;
}

WindowTreeImpl* ConnectionManager::EmbedAtWindow(
    ServerWindow* root,
    uint32_t policy_bitmask,
    mojom::WindowTreeClientPtr client) {
  scoped_ptr<WindowTreeImpl> tree_ptr(
      new ws::WindowTreeImpl(this, root, policy_bitmask));
  WindowTreeImpl* tree = tree_ptr.get();

  mojom::WindowTreePtr window_tree_ptr;
  scoped_ptr<ClientConnection> client_connection =
      delegate_->CreateClientConnectionForEmbedAtWindow(
          this, tree, GetProxy(&window_tree_ptr), std::move(client));

  AddTree(std::move(tree_ptr), std::move(client_connection),
          std::move(window_tree_ptr));
  OnConnectionMessagedClient(tree->id());
  return tree;
}

WindowTreeImpl* ConnectionManager::AddTree(
    scoped_ptr<WindowTreeImpl> tree_impl_ptr,
    scoped_ptr<ClientConnection> connection,
    mojom::WindowTreePtr tree_ptr) {
  CHECK_EQ(0u, tree_map_.count(tree_impl_ptr->id()));
  WindowTreeImpl* tree = tree_impl_ptr.get();
  tree_map_[tree->id()] = std::move(tree_impl_ptr);
  tree->Init(std::move(connection), std::move(tree_ptr));
  return tree;
}

WindowTreeImpl* ConnectionManager::CreateTreeForWindowManager(
    Display* display,
    mojom::WindowManagerFactory* factory,
    ServerWindow* root) {
  mojom::DisplayPtr display_ptr = DisplayToMojomDisplay(display);
  mojom::WindowTreeClientPtr tree_client;
  factory->CreateWindowManager(std::move(display_ptr), GetProxy(&tree_client));
  scoped_ptr<ws::WindowTreeImpl> tree_ptr(new ws::WindowTreeImpl(
      this, root, mojom::WindowTree::kAccessPolicyEmbedRoot));
  ws::WindowTreeImpl* tree = tree_ptr.get();
  scoped_ptr<ws::DefaultClientConnection> connection(
      new ws::DefaultClientConnection(tree_ptr.get(), this,
                                      std::move(tree_client)));
  mojom::WindowTreePtr window_tree_ptr =
      connection->CreateInterfacePtrAndBind();
  AddTree(std::move(tree_ptr), std::move(connection),
          std::move(window_tree_ptr));
  tree->ConfigureWindowManager();
  return tree;
}

void ConnectionManager::DestroyTree(WindowTreeImpl* tree) {
  scoped_ptr<WindowTreeImpl> tree_ptr;
  {
    auto iter = tree_map_.find(tree->id());
    DCHECK(iter != tree_map_.end());
    tree_ptr = std::move(iter->second);
    tree_map_.erase(iter);
  }

  // Notify remaining connections so that they can cleanup.
  for (auto& pair : tree_map_)
    pair.second->OnWindowDestroyingTreeImpl(tree);

  // Notify the hosts, taking care to only notify each host once.
  std::set<Display*> displays_notified;
  for (auto* root : tree->roots()) {
    Display* display = GetDisplayContaining(root);
    if (display && displays_notified.count(display) == 0) {
      display->OnWindowTreeConnectionError(tree);
      displays_notified.insert(display);
    }
  }

  // Remove any requests from the client that resulted in a call to the window
  // manager and we haven't gotten a response back yet.
  std::set<uint32_t> to_remove;
  for (auto& pair : in_flight_wm_change_map_) {
    if (pair.second.connection_id == tree->id())
      to_remove.insert(pair.first);
  }
  for (uint32_t id : to_remove)
    in_flight_wm_change_map_.erase(id);
}

WindowTreeImpl* ConnectionManager::GetConnection(
    ConnectionSpecificId connection_id) {
  auto iter = tree_map_.find(connection_id);
  return iter == tree_map_.end() ? nullptr : iter->second.get();
}

ServerWindow* ConnectionManager::GetWindow(const WindowId& id) {
  // kInvalidConnectionId is used for Display and WindowManager nodes.
  if (id.connection_id == kInvalidConnectionId) {
    for (Display* display : displays_) {
      ServerWindow* window = display->GetRootWithId(id);
      if (window)
        return window;
    }
  }
  WindowTreeImpl* service = GetConnection(id.connection_id);
  return service ? service->GetWindow(id) : nullptr;
}

void ConnectionManager::SchedulePaint(const ServerWindow* window,
                                      const gfx::Rect& bounds) {
  for (Display* display : displays_) {
    if (display->SchedulePaintIfInViewport(window, bounds))
      return;
  }
}

void ConnectionManager::OnDisplayAcceleratedWidgetAvailable(Display* display) {
  DCHECK_NE(0u, pending_displays_.count(display));
  DCHECK_EQ(0u, displays_.count(display));
  const bool is_first_display = displays_.empty();
  displays_.insert(display);
  pending_displays_.erase(display);
  if (is_first_display)
    delegate_->OnFirstDisplayReady();
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
  Display* display = GetDisplayContaining(window);
  if (display)
    return display->GetViewportMetrics().Clone();

  if (!displays_.empty())
    return (*displays_.begin())->GetViewportMetrics().Clone();

  mojom::ViewportMetricsPtr metrics = mojom::ViewportMetrics::New();
  metrics->size_in_pixels = mojo::Size::New();
  return metrics;
}

const WindowTreeImpl* ConnectionManager::GetConnectionWithRoot(
    const ServerWindow* window) const {
  if (!window)
    return nullptr;
  for (auto& pair : tree_map_) {
    if (pair.second->HasRoot(window))
      return pair.second.get();
  }
  return nullptr;
}

WindowManagerAndDisplayConst ConnectionManager::GetWindowManagerAndDisplay(
    const ServerWindow* window) const {
  const ServerWindow* last = window;
  while (window && window->parent()) {
    last = window;
    window = window->parent();
  }
  for (Display* display : displays_) {
    if (window == display->root_window()) {
      WindowManagerAndDisplayConst result;
      result.display = display;
      result.window_manager_state =
          display->GetWindowManagerStateWithRoot(last);
      return result;
    }
  }
  return WindowManagerAndDisplayConst();
}

WindowManagerAndDisplay ConnectionManager::GetWindowManagerAndDisplay(
    const ServerWindow* window) {
  WindowManagerAndDisplayConst result_const =
      const_cast<const ConnectionManager*>(this)->GetWindowManagerAndDisplay(
          window);
  WindowManagerAndDisplay result;
  result.display = const_cast<Display*>(result_const.display);
  result.window_manager_state =
      const_cast<WindowManagerState*>(result_const.window_manager_state);
  return result;
}

Display* ConnectionManager::GetDisplayContaining(const ServerWindow* window) {
  return const_cast<Display*>(
      static_cast<const ConnectionManager*>(this)->GetDisplayContaining(
          window));
}

const Display* ConnectionManager::GetDisplayContaining(
    const ServerWindow* window) const {
  while (window && window->parent())
    window = window->parent();
  for (Display* display : displays_) {
    if (window == display->root_window())
      return display;
  }
  return nullptr;
}

Display* ConnectionManager::GetActiveDisplay() {
  // TODO(sky): this isn't active, but first. Make it active.
  return displays_.size() ? *displays_.begin() : nullptr;
}

void ConnectionManager::AddDisplayManagerBinding(
    mojo::InterfaceRequest<mojom::DisplayManager> request) {
  display_manager_bindings_.AddBinding(this, std::move(request));
}

void ConnectionManager::OnFirstWindowManagerFactorySet() {
  if (!displays_.empty() || !pending_displays_.empty())
    return;

  // We've been supplied a WindowManagerFactory and no displays have been
  // created yet. Treat this as a signal to create a Display.
  // TODO(sky): we need a better way to determine this, most likely a switch.
  delegate_->CreateDefaultDisplays();
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
  InFlightWindowManagerChange change;
  if (!GetAndClearInFlightWindowManagerChange(window_manager_change_id,
                                              &change)) {
    return;
  }

  WindowTreeImpl* connection = GetConnection(change.connection_id);
  connection->OnChangeCompleted(change.client_change_id, success);
}

void ConnectionManager::WindowManagerCreatedTopLevelWindow(
    WindowTreeImpl* wm_connection,
    uint32_t window_manager_change_id,
    const ServerWindow* window) {
  InFlightWindowManagerChange change;
  if (!GetAndClearInFlightWindowManagerChange(window_manager_change_id,
                                              &change)) {
    return;
  }
  if (!window) {
    WindowManagerSentBogusMessage();
    return;
  }

  WindowTreeImpl* connection = GetConnection(change.connection_id);
  // The window manager should have created the window already, and it should
  // be ready for embedding.
  if (!connection->IsWaitingForNewTopLevelWindow(window_manager_change_id) ||
      !window || window->id().connection_id != wm_connection->id() ||
      !window->children().empty() || GetConnectionWithRoot(window)) {
    WindowManagerSentBogusMessage();
    return;
  }

  connection->OnWindowManagerCreatedTopLevelWindow(
      window_manager_change_id, change.client_change_id, window);
}

mojom::DisplayPtr ConnectionManager::DisplayToMojomDisplay(Display* display) {
  size_t i = 0;
  int next_x = 0;
  for (Display* display2 : displays_) {
    const ServerWindow* root = display->root_window();
    if (display == display2) {
      mojom::DisplayPtr display_ptr = mojom::Display::New();
      display_ptr = mojom::Display::New();
      display_ptr->id = display->id();
      display_ptr->bounds = mojo::Rect::New();
      display_ptr->bounds->x = next_x;
      display_ptr->bounds->y = 0;
      display_ptr->bounds->width = root->bounds().size().width();
      display_ptr->bounds->height = root->bounds().size().height();
      // TODO(sky): window manager needs an API to set the work area.
      display_ptr->work_area = display_ptr->bounds.Clone();
      display_ptr->device_pixel_ratio =
          display->GetViewportMetrics().device_pixel_ratio;
      display_ptr->rotation = display->GetRotation();
      // TODO(sky): make this real.
      display_ptr->is_primary = i == 0;
      // TODO(sky): make this real.
      display_ptr->touch_support = mojom::TouchSupport::UNKNOWN;
      display_ptr->frame_decoration_values =
          display->frame_decoration_values().Clone();
      return display_ptr;
    }
    next_x += root->bounds().size().width();
    ++i;
  }
  NOTREACHED();
  return mojom::Display::New();
}

void ConnectionManager::ProcessWindowBoundsChanged(
    const ServerWindow* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowBoundsChanged(window, old_bounds, new_bounds,
                                            IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessClientAreaChanged(
    const ServerWindow* window,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessClientAreaChanged(window, new_client_area,
                                          new_additional_client_areas,
                                          IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessLostCapture(const ServerWindow* window) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessLostCapture(window, IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessWillChangeWindowHierarchy(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWillChangeWindowHierarchy(
        window, new_parent, old_parent, IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessWindowHierarchyChanged(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowHierarchyChanged(window, new_parent, old_parent,
                                               IsOperationSource(pair.first));
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
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowReorder(window, relative_window, direction,
                                      IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessWindowDeleted(const ServerWindow* window) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowDeleted(window, IsOperationSource(pair.first));
  }
}

void ConnectionManager::ProcessWillChangeWindowPredefinedCursor(
    ServerWindow* window,
    int32_t cursor_id) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessCursorChanged(window, cursor_id,
                                      IsOperationSource(pair.first));
  }

  // Pass the cursor change to the native window.
  Display* display = GetDisplayContaining(window);
  if (display)
    display->OnCursorUpdated(window);
}

void ConnectionManager::ProcessViewportMetricsChanged(
    Display* display,
    const mojom::ViewportMetrics& old_metrics,
    const mojom::ViewportMetrics& new_metrics) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessViewportMetricsChanged(
        display, old_metrics, new_metrics, IsOperationSource(pair.first));
  }

  if (!got_valid_frame_decorations_)
    return;
}

void ConnectionManager::ProcessFrameDecorationValuesChanged(Display* display) {
  if (!got_valid_frame_decorations_) {
    got_valid_frame_decorations_ = true;
    display_manager_observers_.ForAllPtrs([this](
        mojom::DisplayManagerObserver* observer) { CallOnDisplays(observer); });
    return;
  }

  display_manager_observers_.ForAllPtrs(
      [this, &display](mojom::DisplayManagerObserver* observer) {
        CallOnDisplayChanged(observer, display);
      });
}

bool ConnectionManager::GetAndClearInFlightWindowManagerChange(
    uint32_t window_manager_change_id,
    InFlightWindowManagerChange* change) {
  // There are valid reasons as to why we wouldn't know about the id. The
  // most likely is the client disconnected before the response from the window
  // manager came back.
  auto iter = in_flight_wm_change_map_.find(window_manager_change_id);
  if (iter == in_flight_wm_change_map_.end())
    return false;

  *change = iter->second;
  in_flight_wm_change_map_.erase(iter);
  return true;
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

void ConnectionManager::MaybeUpdateNativeCursor(ServerWindow* window) {
  // This can be null in unit tests.
  Display* display = GetDisplayContaining(window);
  if (display)
    display->MaybeChangeCursorOnWindowTreeChange();
}

void ConnectionManager::CallOnDisplays(
    mojom::DisplayManagerObserver* observer) {
  mojo::Array<mojom::DisplayPtr> displays(displays_.size());
  {
    size_t i = 0;
    // TODO(sky): need ordering!
    for (Display* display : displays_) {
      displays[i] = DisplayToMojomDisplay(display);
      ++i;
    }
  }
  observer->OnDisplays(std::move(displays));
}

void ConnectionManager::CallOnDisplayChanged(
    mojom::DisplayManagerObserver* observer,
    Display* display) {
  mojo::Array<mojom::DisplayPtr> displays(1);
  displays[0] = DisplayToMojomDisplay(display);
  display_manager_observers_.ForAllPtrs(
      [&displays](mojom::DisplayManagerObserver* observer) {
        observer->OnDisplaysChanged(displays.Clone());
      });
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
  const Display* display = GetDisplayContaining(window);
  return display ? display->root_window() : nullptr;
}

void ConnectionManager::ScheduleSurfaceDestruction(ServerWindow* window) {
  for (Display* display : displays_) {
    if (display->root_window()->Contains(window)) {
      display->ScheduleSurfaceDestruction(window);
      break;
    }
  }
}

ServerWindow* ConnectionManager::FindWindowForSurface(
    const ServerWindow* ancestor,
    mojom::SurfaceType surface_type,
    const ClientWindowId& client_window_id) {
  WindowTreeImpl* window_tree;
  if (ancestor->id().connection_id == kInvalidConnectionId) {
    WindowManagerAndDisplay wm_and_display =
        GetWindowManagerAndDisplay(ancestor);
    window_tree = wm_and_display.window_manager_state
                      ? wm_and_display.window_manager_state->tree()
                      : nullptr;
  } else {
    window_tree = GetConnection(ancestor->id().connection_id);
  }
  if (!window_tree)
    return nullptr;
  if (surface_type == mojom::SurfaceType::DEFAULT) {
    // At embed points the default surface comes from the embedded app.
    WindowTreeImpl* connection_with_root = GetConnectionWithRoot(ancestor);
    if (connection_with_root)
      window_tree = connection_with_root;
  }
  return window_tree->GetWindowByClientId(client_window_id);
}

void ConnectionManager::OnWindowDestroyed(ServerWindow* window) {
  if (!in_destructor_)
    ProcessWindowDeleted(window);
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

  MaybeUpdateNativeCursor(window);
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

  MaybeUpdateNativeCursor(window);
}

void ConnectionManager::OnWindowClientAreaChanged(
    ServerWindow* window,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas) {
  if (in_destructor_)
    return;

  ProcessClientAreaChanged(window, new_client_area,
                           new_additional_client_areas);
}

void ConnectionManager::OnWindowReordered(ServerWindow* window,
                                          ServerWindow* relative,
                                          mojom::OrderDirection direction) {
  ProcessWindowReorder(window, relative, direction);
  if (!in_destructor_)
    SchedulePaint(window, gfx::Rect(window->bounds().size()));
  MaybeUpdateNativeCursor(window);
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

  for (auto& pair : tree_map_) {
    pair.second->ProcessWillChangeWindowVisibility(
        window, IsOperationSource(pair.first));
  }
}

void ConnectionManager::OnWindowPredefinedCursorChanged(ServerWindow* window,
                                                        int32_t cursor_id) {
  if (in_destructor_)
    return;

  ProcessWillChangeWindowPredefinedCursor(window, cursor_id);
}

void ConnectionManager::OnWindowSharedPropertyChanged(
    ServerWindow* window,
    const std::string& name,
    const std::vector<uint8_t>* new_data) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowPropertyChanged(window, name, new_data,
                                              IsOperationSource(pair.first));
  }
}

void ConnectionManager::OnWindowTextInputStateChanged(
    ServerWindow* window,
    const ui::TextInputState& state) {
  Display* display = GetDisplayContaining(window);
  display->UpdateTextInputState(window, state);
}

void ConnectionManager::OnTransientWindowAdded(ServerWindow* window,
                                               ServerWindow* transient_child) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessTransientWindowAdded(window, transient_child,
                                             IsOperationSource(pair.first));
  }
}

void ConnectionManager::OnTransientWindowRemoved(
    ServerWindow* window,
    ServerWindow* transient_child) {
  // If we're deleting a window, then this is a superfluous message.
  if (current_operation_type() == OperationType::DELETE_WINDOW)
    return;
  for (auto& pair : tree_map_) {
    pair.second->ProcessTransientWindowRemoved(window, transient_child,
                                               IsOperationSource(pair.first));
  }
}

void ConnectionManager::AddObserver(mojom::DisplayManagerObserverPtr observer) {
  // TODO(sky): this needs to be per user.

  // Many clients key off the frame decorations to size widgets. Wait for frame
  // decorations before notifying so that we don't have to worry about clients
  // resizing appropriately.
  if (!got_valid_frame_decorations_) {
    display_manager_observers_.AddInterfacePtr(std::move(observer));
    return;
  }
  CallOnDisplays(observer.get());
  display_manager_observers_.AddInterfacePtr(std::move(observer));
}

}  // namespace ws
}  // namespace mus
