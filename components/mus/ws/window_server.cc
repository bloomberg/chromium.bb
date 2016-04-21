// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_server.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/operation.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "components/mus/ws/window_manager_access_policy.h"
#include "components/mus/ws/window_manager_factory_service.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_server_delegate.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "services/shell/public/cpp/connection.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace mus {
namespace ws {

WindowServer::WindowServer(
    WindowServerDelegate* delegate,
    const scoped_refptr<mus::SurfacesState>& surfaces_state)
    : delegate_(delegate),
      surfaces_state_(surfaces_state),
      next_connection_id_(1),
      display_manager_(new DisplayManager(this)),
      current_operation_(nullptr),
      in_destructor_(false),
      next_wm_change_id_(0),
      window_manager_factory_registry_(this, &user_id_tracker_) {}

WindowServer::~WindowServer() {
  in_destructor_ = true;

  // Destroys the window trees results in querying for the display. Tear down
  // the displays first so that the trees are notified of the display going
  // away while the display is still valid.
  display_manager_->DestroyAllDisplays();

  while (!tree_map_.empty())
    DestroyTree(tree_map_.begin()->second.get());

  display_manager_.reset();
}

ServerWindow* WindowServer::CreateServerWindow(
    const WindowId& id,
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  ServerWindow* window = new ServerWindow(this, id, properties);
  window->AddObserver(this);
  return window;
}

ConnectionSpecificId WindowServer::GetAndAdvanceNextConnectionId() {
  const ConnectionSpecificId id = next_connection_id_++;
  DCHECK_LT(id, next_connection_id_);
  return id;
}

WindowTree* WindowServer::EmbedAtWindow(
    ServerWindow* root,
    const UserId& user_id,
    mojom::WindowTreeClientPtr client,
    std::unique_ptr<AccessPolicy> access_policy) {
  std::unique_ptr<WindowTree> tree_ptr(
      new WindowTree(this, user_id, root, std::move(access_policy)));
  WindowTree* tree = tree_ptr.get();

  mojom::WindowTreePtr window_tree_ptr;
  mojom::WindowTreeRequest window_tree_request = GetProxy(&window_tree_ptr);
  std::unique_ptr<WindowTreeBinding> binding =
      delegate_->CreateWindowTreeBinding(
          WindowServerDelegate::BindingType::EMBED, this, tree,
          &window_tree_request, &client);
  if (!binding) {
    binding.reset(new ws::DefaultWindowTreeBinding(
        tree, this, std::move(window_tree_request), std::move(client)));
  }

  AddTree(std::move(tree_ptr), std::move(binding), std::move(window_tree_ptr));
  OnTreeMessagedClient(tree->id());
  return tree;
}

WindowTree* WindowServer::AddTree(std::unique_ptr<WindowTree> tree_impl_ptr,
                                  std::unique_ptr<WindowTreeBinding> binding,
                                  mojom::WindowTreePtr tree_ptr) {
  CHECK_EQ(0u, tree_map_.count(tree_impl_ptr->id()));
  WindowTree* tree = tree_impl_ptr.get();
  tree_map_[tree->id()] = std::move(tree_impl_ptr);
  tree->Init(std::move(binding), std::move(tree_ptr));
  return tree;
}

WindowTree* WindowServer::CreateTreeForWindowManager(
    Display* display,
    mojom::WindowManagerFactory* factory,
    ServerWindow* root,
    const UserId& user_id) {
  mojom::DisplayPtr display_ptr = display->ToMojomDisplay();
  mojom::WindowTreeClientPtr tree_client;
  factory->CreateWindowManager(std::move(display_ptr), GetProxy(&tree_client));
  std::unique_ptr<WindowTree> tree_ptr(new WindowTree(
      this, user_id, root, base::WrapUnique(new WindowManagerAccessPolicy)));
  WindowTree* tree = tree_ptr.get();
  mojom::WindowTreePtr window_tree_ptr;
  mojom::WindowTreeRequest tree_request;
  std::unique_ptr<WindowTreeBinding> binding =
      delegate_->CreateWindowTreeBinding(
          WindowServerDelegate::BindingType::WINDOW_MANAGER, this, tree,
          &tree_request, &tree_client);
  if (!binding) {
    DefaultWindowTreeBinding* default_binding = new DefaultWindowTreeBinding(
        tree_ptr.get(), this, std::move(tree_client));
    binding.reset(default_binding);
    window_tree_ptr = default_binding->CreateInterfacePtrAndBind();
  }
  AddTree(std::move(tree_ptr), std::move(binding), std::move(window_tree_ptr));
  tree->ConfigureWindowManager();
  return tree;
}

void WindowServer::DestroyTree(WindowTree* tree) {
  std::unique_ptr<WindowTree> tree_ptr;
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
    // WindowTree holds its roots as a const, which is right as WindowTree
    // doesn't need to modify the window. OTOH we do. We could look up the
    // window using the id to get non-const version, but instead we cast.
    Display* display =
        display_manager_->GetDisplayContaining(const_cast<ServerWindow*>(root));
    if (display && displays_notified.count(display) == 0) {
      display->OnWillDestroyTree(tree);
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

WindowTree* WindowServer::GetTreeWithId(ConnectionSpecificId connection_id) {
  auto iter = tree_map_.find(connection_id);
  return iter == tree_map_.end() ? nullptr : iter->second.get();
}

WindowTree* WindowServer::GetTreeWithConnectionName(
    const std::string& connection_name) {
  for (const auto& entry : tree_map_) {
    if (entry.second->connection_name() == connection_name)
      return entry.second.get();
  }
  return nullptr;
}

ServerWindow* WindowServer::GetWindow(const WindowId& id) {
  // kInvalidConnectionId is used for Display and WindowManager nodes.
  if (id.connection_id == kInvalidConnectionId) {
    for (Display* display : display_manager_->displays()) {
      ServerWindow* window = display->GetRootWithId(id);
      if (window)
        return window;
    }
  }
  WindowTree* tree = GetTreeWithId(id.connection_id);
  return tree ? tree->GetWindow(id) : nullptr;
}

void WindowServer::SchedulePaint(ServerWindow* window,
                                 const gfx::Rect& bounds) {
  Display* display = display_manager_->GetDisplayContaining(window);
  if (display)
    display->SchedulePaint(window, bounds);
}

void WindowServer::OnTreeMessagedClient(ConnectionSpecificId id) {
  if (current_operation_)
    current_operation_->MarkTreeAsMessaged(id);
}

bool WindowServer::DidTreeMessageClient(ConnectionSpecificId id) const {
  return current_operation_ && current_operation_->DidMessageTree(id);
}

mojom::ViewportMetricsPtr WindowServer::GetViewportMetricsForWindow(
    const ServerWindow* window) {
  const Display* display = display_manager_->GetDisplayContaining(window);
  if (display)
    return display->GetViewportMetrics().Clone();

  if (!display_manager_->displays().empty())
    return (*display_manager_->displays().begin())
        ->GetViewportMetrics()
        .Clone();

  mojom::ViewportMetricsPtr metrics = mojom::ViewportMetrics::New();
  metrics->size_in_pixels = mojo::Size::New();
  return metrics;
}

const WindowTree* WindowServer::GetTreeWithRoot(
    const ServerWindow* window) const {
  if (!window)
    return nullptr;
  for (auto& pair : tree_map_) {
    if (pair.second->HasRoot(window))
      return pair.second.get();
  }
  return nullptr;
}

void WindowServer::OnFirstWindowManagerFactorySet() {
  if (display_manager_->has_active_or_pending_displays())
    return;

  // We've been supplied a WindowManagerFactory and no displays have been
  // created yet. Treat this as a signal to create a Display.
  // TODO(sky): we need a better way to determine this, most likely a switch.
  delegate_->CreateDefaultDisplays();
}

bool WindowServer::SetFocusedWindow(ServerWindow* window) {
  // TODO(sky): this should fail if there is modal dialog active and |window|
  // is outside that.
  ServerWindow* currently_focused = GetFocusedWindow();
  Display* focused_display =
      currently_focused
          ? display_manager_->GetDisplayContaining(currently_focused)
          : nullptr;
  if (!window)
    return focused_display ? focused_display->SetFocusedWindow(nullptr) : true;

  Display* display = display_manager_->GetDisplayContaining(window);
  DCHECK(display);  // It's assumed callers do validation before calling this.
  const bool result = display->SetFocusedWindow(window);
  // If the focus actually changed, and focus was in another display, then we
  // need to notify the previously focused display so that it cleans up state
  // and notifies appropriately.
  if (result && display->GetFocusedWindow() && display != focused_display &&
      focused_display) {
    const bool cleared_focus = focused_display->SetFocusedWindow(nullptr);
    DCHECK(cleared_focus);
  }
  return result;
}

ServerWindow* WindowServer::GetFocusedWindow() {
  for (Display* display : display_manager_->displays()) {
    ServerWindow* focused_window = display->GetFocusedWindow();
    if (focused_window)
      return focused_window;
  }
  return nullptr;
}

uint32_t WindowServer::GenerateWindowManagerChangeId(
    WindowTree* source,
    uint32_t client_change_id) {
  const uint32_t wm_change_id = next_wm_change_id_++;
  in_flight_wm_change_map_[wm_change_id] = {source->id(), client_change_id};
  return wm_change_id;
}

void WindowServer::WindowManagerChangeCompleted(
    uint32_t window_manager_change_id,
    bool success) {
  InFlightWindowManagerChange change;
  if (!GetAndClearInFlightWindowManagerChange(window_manager_change_id,
                                              &change)) {
    return;
  }

  WindowTree* tree = GetTreeWithId(change.connection_id);
  tree->OnChangeCompleted(change.client_change_id, success);
}

void WindowServer::WindowManagerCreatedTopLevelWindow(
    WindowTree* wm_tree,
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

  WindowTree* tree = GetTreeWithId(change.connection_id);
  // The window manager should have created the window already, and it should
  // be ready for embedding.
  if (!tree->IsWaitingForNewTopLevelWindow(window_manager_change_id) ||
      !window || window->id().connection_id != wm_tree->id() ||
      !window->children().empty() || GetTreeWithRoot(window)) {
    WindowManagerSentBogusMessage();
    return;
  }

  tree->OnWindowManagerCreatedTopLevelWindow(window_manager_change_id,
                                             change.client_change_id, window);
}

void WindowServer::ProcessWindowBoundsChanged(const ServerWindow* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowBoundsChanged(window, old_bounds, new_bounds,
                                            IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessClientAreaChanged(
    const ServerWindow* window,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessClientAreaChanged(window, new_client_area,
                                          new_additional_client_areas,
                                          IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessLostCapture(const ServerWindow* window) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessLostCapture(window, IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessWillChangeWindowHierarchy(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWillChangeWindowHierarchy(
        window, new_parent, old_parent, IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessWindowHierarchyChanged(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowHierarchyChanged(window, new_parent, old_parent,
                                               IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessWindowReorder(const ServerWindow* window,
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

void WindowServer::ProcessWindowDeleted(const ServerWindow* window) {
  for (auto& pair : tree_map_)
    pair.second->ProcessWindowDeleted(window, IsOperationSource(pair.first));
}

void WindowServer::ProcessWillChangeWindowPredefinedCursor(ServerWindow* window,
                                                           int32_t cursor_id) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessCursorChanged(window, cursor_id,
                                      IsOperationSource(pair.first));
  }

  // Pass the cursor change to the native window.
  Display* display = display_manager_->GetDisplayContaining(window);
  if (display)
    display->OnCursorUpdated(window);
}

void WindowServer::SetPaintCallback(
    const base::Callback<void(ServerWindow*)>& callback) {
  DCHECK(delegate_->IsTestConfig()) << "Paint callbacks are expensive, and "
                                    << "allowed only in tests.";
  DCHECK(window_paint_callback_.is_null() || callback.is_null());
  window_paint_callback_ = callback;
}

void WindowServer::ProcessViewportMetricsChanged(
    Display* display,
    const mojom::ViewportMetrics& old_metrics,
    const mojom::ViewportMetrics& new_metrics) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessViewportMetricsChanged(
        display, old_metrics, new_metrics, IsOperationSource(pair.first));
  }
}

bool WindowServer::GetAndClearInFlightWindowManagerChange(
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

void WindowServer::PrepareForOperation(Operation* op) {
  // Should only ever have one change in flight.
  CHECK(!current_operation_);
  current_operation_ = op;
}

void WindowServer::FinishOperation() {
  // PrepareForOperation/FinishOperation should be balanced.
  CHECK(current_operation_);
  current_operation_ = nullptr;
}

void WindowServer::MaybeUpdateNativeCursor(ServerWindow* window) {
  // This can be null in unit tests.
  Display* display = display_manager_->GetDisplayContaining(window);
  if (display)
    display->MaybeChangeCursorOnWindowTreeChange();
}

mus::SurfacesState* WindowServer::GetSurfacesState() {
  return surfaces_state_.get();
}

void WindowServer::OnScheduleWindowPaint(ServerWindow* window) {
  if (in_destructor_)
    return;

  SchedulePaint(window, gfx::Rect(window->bounds().size()));
  if (!window_paint_callback_.is_null())
    window_paint_callback_.Run(window);
}

const ServerWindow* WindowServer::GetRootWindow(
    const ServerWindow* window) const {
  const Display* display = display_manager_->GetDisplayContaining(window);
  return display ? display->root_window() : nullptr;
}

void WindowServer::ScheduleSurfaceDestruction(ServerWindow* window) {
  Display* display = display_manager_->GetDisplayContaining(window);
  if (display)
    display->ScheduleSurfaceDestruction(window);
}

ServerWindow* WindowServer::FindWindowForSurface(
    const ServerWindow* ancestor,
    mojom::SurfaceType surface_type,
    const ClientWindowId& client_window_id) {
  WindowTree* window_tree;
  if (ancestor->id().connection_id == kInvalidConnectionId) {
    WindowManagerAndDisplay wm_and_display =
        display_manager_->GetWindowManagerAndDisplay(ancestor);
    window_tree = wm_and_display.window_manager_state
                      ? wm_and_display.window_manager_state->tree()
                      : nullptr;
  } else {
    window_tree = GetTreeWithId(ancestor->id().connection_id);
  }
  if (!window_tree)
    return nullptr;
  if (surface_type == mojom::SurfaceType::DEFAULT) {
    // At embed points the default surface comes from the embedded app.
    WindowTree* tree_with_root = GetTreeWithRoot(ancestor);
    if (tree_with_root)
      window_tree = tree_with_root;
  }
  return window_tree->GetWindowByClientId(client_window_id);
}

void WindowServer::OnWindowDestroyed(ServerWindow* window) {
  ProcessWindowDeleted(window);
}

void WindowServer::OnWillChangeWindowHierarchy(ServerWindow* window,
                                               ServerWindow* new_parent,
                                               ServerWindow* old_parent) {
  if (in_destructor_)
    return;

  ProcessWillChangeWindowHierarchy(window, new_parent, old_parent);
}

void WindowServer::OnWindowHierarchyChanged(ServerWindow* window,
                                            ServerWindow* new_parent,
                                            ServerWindow* old_parent) {
  if (in_destructor_)
    return;

  WindowManagerState* wms =
      display_manager_->GetWindowManagerAndDisplay(window).window_manager_state;
  if (wms)
    wms->ReleaseCaptureBlockedByAnyModalWindow();

  ProcessWindowHierarchyChanged(window, new_parent, old_parent);

  // TODO(beng): optimize.
  if (old_parent)
    SchedulePaint(old_parent, gfx::Rect(old_parent->bounds().size()));
  if (new_parent)
    SchedulePaint(new_parent, gfx::Rect(new_parent->bounds().size()));

  MaybeUpdateNativeCursor(window);
}

void WindowServer::OnWindowBoundsChanged(ServerWindow* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  if (in_destructor_)
    return;

  ProcessWindowBoundsChanged(window, old_bounds, new_bounds);
  if (!window->parent())
    return;

  SchedulePaint(window->parent(), old_bounds);
  SchedulePaint(window->parent(), new_bounds);

  MaybeUpdateNativeCursor(window);
}

void WindowServer::OnWindowClientAreaChanged(
    ServerWindow* window,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas) {
  if (in_destructor_)
    return;

  ProcessClientAreaChanged(window, new_client_area,
                           new_additional_client_areas);
}

void WindowServer::OnWindowReordered(ServerWindow* window,
                                     ServerWindow* relative,
                                     mojom::OrderDirection direction) {
  ProcessWindowReorder(window, relative, direction);
  if (!in_destructor_)
    SchedulePaint(window, gfx::Rect(window->bounds().size()));
  MaybeUpdateNativeCursor(window);
}

void WindowServer::OnWillChangeWindowVisibility(ServerWindow* window) {
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

void WindowServer::OnWindowOpacityChanged(ServerWindow* window,
                                          float old_opacity,
                                          float new_opacity) {
  DCHECK(!in_destructor_);

  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowOpacityChanged(window, old_opacity, new_opacity,
                                             IsOperationSource(pair.first));
  }
}

void WindowServer::OnWindowVisibilityChanged(ServerWindow* window) {
  if (in_destructor_)
    return;

  WindowManagerState* wms =
      display_manager_->GetWindowManagerAndDisplay(window).window_manager_state;
  if (wms)
    wms->ReleaseCaptureBlockedByModalWindow(window);
}

void WindowServer::OnWindowPredefinedCursorChanged(ServerWindow* window,
                                                   int32_t cursor_id) {
  if (in_destructor_)
    return;

  ProcessWillChangeWindowPredefinedCursor(window, cursor_id);
}

void WindowServer::OnWindowSharedPropertyChanged(
    ServerWindow* window,
    const std::string& name,
    const std::vector<uint8_t>* new_data) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowPropertyChanged(window, name, new_data,
                                              IsOperationSource(pair.first));
  }
}

void WindowServer::OnWindowTextInputStateChanged(
    ServerWindow* window,
    const ui::TextInputState& state) {
  Display* display = display_manager_->GetDisplayContaining(window);
  display->UpdateTextInputState(window, state);
}

void WindowServer::OnTransientWindowAdded(ServerWindow* window,
                                          ServerWindow* transient_child) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessTransientWindowAdded(window, transient_child,
                                             IsOperationSource(pair.first));
  }
}

void WindowServer::OnTransientWindowRemoved(ServerWindow* window,
                                            ServerWindow* transient_child) {
  // If we're deleting a window, then this is a superfluous message.
  if (current_operation_type() == OperationType::DELETE_WINDOW)
    return;
  for (auto& pair : tree_map_) {
    pair.second->ProcessTransientWindowRemoved(window, transient_child,
                                               IsOperationSource(pair.first));
  }
}

void WindowServer::OnFirstDisplayReady() {
  delegate_->OnFirstDisplayReady();
}

void WindowServer::OnNoMoreDisplays() {
  delegate_->OnNoMoreDisplays();
}

}  // namespace ws
}  // namespace mus
