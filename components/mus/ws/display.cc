// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/display.h"

#include "base/debug/debugger.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mus/common/types.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/focus_controller.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/window_manager_factory_service.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"

namespace mus {
namespace ws {

Display::Display(ConnectionManager* connection_manager,
                 mojo::Connector* connector,
                 const scoped_refptr<GpuState>& gpu_state,
                 const scoped_refptr<SurfacesState>& surfaces_state)
    : id_(connection_manager->display_manager()->GetAndAdvanceNextDisplayId()),
      connection_manager_(connection_manager),
      platform_display_(
          PlatformDisplay::Create(connector, gpu_state, surfaces_state)),
      last_cursor_(0) {
  platform_display_->Init(this);

  connection_manager_->window_manager_factory_registry()->AddObserver(this);
  connection_manager_->user_id_tracker()->AddObserver(this);
}

Display::~Display() {
  connection_manager_->user_id_tracker()->RemoveObserver(this);

  connection_manager_->window_manager_factory_registry()->RemoveObserver(this);

  DestroyFocusController();
  for (ServerWindow* window : windows_needing_frame_destruction_)
    window->RemoveObserver(this);

  // Destroy any trees, which triggers destroying the WindowManagerState. Copy
  // off the WindowManagerStates as destruction mutates
  // |window_manager_state_map_|.
  std::set<WindowManagerState*> states;
  for (auto& pair : window_manager_state_map_)
    states.insert(pair.second.get());
  for (WindowManagerState* state : states)
    connection_manager_->DestroyTree(state->tree());
}

void Display::Init(scoped_ptr<DisplayBinding> binding) {
  init_called_ = true;
  binding_ = std::move(binding);
  display_manager()->AddDisplay(this);
  InitWindowManagersIfNecessary();
}

DisplayManager* Display::display_manager() {
  return connection_manager_->display_manager();
}

const DisplayManager* Display::display_manager() const {
  return connection_manager_->display_manager();
}

mojom::DisplayPtr Display::ToMojomDisplay() const {
  mojom::DisplayPtr display_ptr = mojom::Display::New();
  display_ptr = mojom::Display::New();
  display_ptr->id = id_;
  display_ptr->bounds = mojo::Rect::New();
  // TODO(sky): Display should know it's origin.
  display_ptr->bounds->x = 0;
  display_ptr->bounds->y = 0;
  display_ptr->bounds->width = root_->bounds().size().width();
  display_ptr->bounds->height = root_->bounds().size().height();
  // TODO(sky): window manager needs an API to set the work area.
  display_ptr->work_area = display_ptr->bounds.Clone();
  display_ptr->device_pixel_ratio =
      platform_display_->GetViewportMetrics().device_pixel_ratio;
  display_ptr->rotation = platform_display_->GetRotation();
  // TODO(sky): make this real.
  display_ptr->is_primary = true;
  // TODO(sky): make this real.
  display_ptr->touch_support = mojom::TouchSupport::UNKNOWN;
  display_ptr->frame_decoration_values = mojom::FrameDecorationValues::New();
  display_ptr->frame_decoration_values->normal_client_area_insets =
      mojo::Insets::New();
  display_ptr->frame_decoration_values->maximized_client_area_insets =
      mojo::Insets::New();
  return display_ptr;
}

void Display::SchedulePaint(const ServerWindow* window,
                            const gfx::Rect& bounds) {
  DCHECK(root_->Contains(window));
  platform_display_->SchedulePaint(window, bounds);
}

void Display::ScheduleSurfaceDestruction(ServerWindow* window) {
  if (!platform_display_->IsFramePending()) {
    window->DestroySurfacesScheduledForDestruction();
    return;
  }
  if (windows_needing_frame_destruction_.count(window))
    return;
  windows_needing_frame_destruction_.insert(window);
  window->AddObserver(this);
}

const mojom::ViewportMetrics& Display::GetViewportMetrics() const {
  return platform_display_->GetViewportMetrics();
}

ServerWindow* Display::GetRootWithId(const WindowId& id) {
  if (id == root_->id())
    return root_.get();
  for (auto& pair : window_manager_state_map_) {
    if (pair.second->root()->id() == id)
      return pair.second->root();
  }
  return nullptr;
}

WindowManagerState* Display::GetWindowManagerStateWithRoot(
    const ServerWindow* window) {
  for (auto& pair : window_manager_state_map_) {
    if (pair.second->root() == window)
      return pair.second.get();
  }
  return nullptr;
}

WindowManagerState* Display::GetFirstWindowManagerState() {
  return window_manager_state_map_.empty()
             ? nullptr
             : window_manager_state_map_.begin()->second.get();
}

const WindowManagerState* Display::GetWindowManagerStateForUser(
    const UserId& user_id) const {
  auto iter = window_manager_state_map_.find(user_id);
  return iter == window_manager_state_map_.end() ? nullptr : iter->second.get();
}

mojom::Rotation Display::GetRotation() const {
  return platform_display_->GetRotation();
}

const WindowManagerState* Display::GetActiveWindowManagerState() const {
  return GetWindowManagerStateForUser(
      connection_manager_->user_id_tracker()->active_id());
}

void Display::SetFocusedWindow(ServerWindow* new_focused_window) {
  // TODO(sky): this is wrong. Focus is global, not per Display.
  ServerWindow* old_focused_window = focus_controller_->GetFocusedWindow();
  if (old_focused_window == new_focused_window)
    return;
  DCHECK(root_window()->Contains(new_focused_window));
  focus_controller_->SetFocusedWindow(new_focused_window);
}

ServerWindow* Display::GetFocusedWindow() {
  return focus_controller_->GetFocusedWindow();
}

void Display::DestroyFocusController() {
  if (!focus_controller_)
    return;

  focus_controller_->RemoveObserver(this);
  focus_controller_.reset();
}

void Display::AddActivationParent(ServerWindow* window) {
  activation_parents_.Add(window);
}

void Display::RemoveActivationParent(ServerWindow* window) {
  activation_parents_.Remove(window);
}

void Display::UpdateTextInputState(ServerWindow* window,
                                   const ui::TextInputState& state) {
  // Do not need to update text input for unfocused windows.
  if (!platform_display_ || focus_controller_->GetFocusedWindow() != window)
    return;
  platform_display_->UpdateTextInputState(state);
}

void Display::SetImeVisibility(ServerWindow* window, bool visible) {
  // Do not need to show or hide IME for unfocused window.
  if (focus_controller_->GetFocusedWindow() != window)
    return;
  platform_display_->SetImeVisibility(visible);
}

void Display::OnWillDestroyTree(WindowTree* tree) {
  for (auto it = window_manager_state_map_.begin();
       it != window_manager_state_map_.end(); ++it) {
    if (it->second->tree() == tree) {
      window_manager_state_map_.erase(it);
      break;
    }
  }

  for (const auto& pair : window_manager_state_map_)
    pair.second->OnWillDestroyTree(tree);
}

void Display::UpdateNativeCursor(int32_t cursor_id) {
  if (cursor_id != last_cursor_) {
    platform_display_->SetCursorById(cursor_id);
    last_cursor_ = cursor_id;
  }
}

void Display::OnCursorUpdated(ServerWindow* window) {
  WindowManagerState* wms = GetActiveWindowManagerState();
  if (wms && window == wms->event_dispatcher()->mouse_cursor_source_window())
    UpdateNativeCursor(window->cursor());
}

void Display::MaybeChangeCursorOnWindowTreeChange() {
  WindowManagerState* wms = GetActiveWindowManagerState();
  if (!wms)
    return;
  wms->event_dispatcher()->UpdateCursorProviderByLastKnownLocation();
  ServerWindow* cursor_source_window =
      wms->event_dispatcher()->mouse_cursor_source_window();
  if (cursor_source_window)
    UpdateNativeCursor(cursor_source_window->cursor());
}

void Display::SetSize(mojo::SizePtr size) {
  platform_display_->SetViewportSize(size.To<gfx::Size>());
}

void Display::SetTitle(const mojo::String& title) {
  platform_display_->SetTitle(title.To<base::string16>());
}

void Display::InitWindowManagersIfNecessary() {
  if (!init_called_ || !root_)
    return;

  display_manager()->OnDisplayAcceleratedWidgetAvailable(this);
  if (binding_) {
    scoped_ptr<WindowManagerState> wms_ptr(new WindowManagerState(
        this, platform_display_.get(), top_level_surface_id_));
    WindowManagerState* wms = wms_ptr.get();
    // For this case we never create additional WindowManagerStates, so any
    // id works.
    window_manager_state_map_[mojo::shell::mojom::kRootUserID] =
        std::move(wms_ptr);
    wms->tree_ = binding_->CreateWindowTree(wms->root());
  } else {
    CreateWindowManagerStatesFromRegistry();
  }
}

void Display::CreateWindowManagerStatesFromRegistry() {
  std::vector<WindowManagerFactoryService*> services =
      connection_manager_->window_manager_factory_registry()->GetServices();
  for (WindowManagerFactoryService* service : services) {
    if (service->window_manager_factory())
      CreateWindowManagerStateFromService(service);
  }
}

void Display::CreateWindowManagerStateFromService(
    WindowManagerFactoryService* service) {
  scoped_ptr<WindowManagerState> wms_ptr(
      new WindowManagerState(this, platform_display_.get(),
                             top_level_surface_id_, service->user_id()));
  WindowManagerState* wms = wms_ptr.get();
  window_manager_state_map_[service->user_id()] = std::move(wms_ptr);
  wms->tree_ = connection_manager_->CreateTreeForWindowManager(
      this, service->window_manager_factory(), wms->root());
}

ServerWindow* Display::GetRootWindow() {
  return root_.get();
}

void Display::OnEvent(const ui::Event& event) {
  WindowManagerState* wms = GetActiveWindowManagerState();
  if (wms)
    wms->ProcessEvent(event);
}

void Display::OnNativeCaptureLost() {
  WindowManagerState* state = GetActiveWindowManagerState();
  if (state)
    state->SetCapture(nullptr, false);
}

void Display::OnDisplayClosed() {
  display_manager()->DestroyDisplay(this);
}

void Display::OnViewportMetricsChanged(
    const mojom::ViewportMetrics& old_metrics,
    const mojom::ViewportMetrics& new_metrics) {
  if (!root_) {
    root_.reset(connection_manager_->CreateServerWindow(
        display_manager()->GetAndAdvanceNextRootId(),
        ServerWindow::Properties()));
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
    root_->SetVisible(true);
    focus_controller_.reset(new FocusController(this, root_.get()));
    focus_controller_->AddObserver(this);
    InitWindowManagersIfNecessary();
  } else {
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
    const gfx::Rect wm_bounds(root_->bounds().size());
    for (auto& pair : window_manager_state_map_)
      pair.second->root()->SetBounds(wm_bounds);
  }
  // TODO(sky): if bounds changed, then need to update
  // Display/WindowManagerState appropriately (e.g. notify observers).
  connection_manager_->ProcessViewportMetricsChanged(this, old_metrics,
                                                     new_metrics);
}

void Display::OnTopLevelSurfaceChanged(cc::SurfaceId surface_id) {
  DCHECK(!root_);
  // This should only be called once, and before we've created root_.
  top_level_surface_id_ = surface_id;
}

void Display::OnCompositorFrameDrawn() {
  std::set<ServerWindow*> windows;
  windows.swap(windows_needing_frame_destruction_);
  for (ServerWindow* window : windows) {
    window->RemoveObserver(this);
    window->DestroySurfacesScheduledForDestruction();
  }
}

bool Display::CanHaveActiveChildren(ServerWindow* window) const {
  return window && activation_parents_.Contains(window);
}

void Display::OnActivationChanged(ServerWindow* old_active_window,
                                  ServerWindow* new_active_window) {
  DCHECK_NE(new_active_window, old_active_window);
  if (new_active_window && new_active_window->parent()) {
    // Raise the new active window.
    // TODO(sad): Let the WM dictate whether to raise the window or not?
    new_active_window->parent()->StackChildAtTop(new_active_window);
  }
}

void Display::OnFocusChanged(FocusControllerChangeSource change_source,
                             ServerWindow* old_focused_window,
                             ServerWindow* new_focused_window) {
  // TODO(sky): focus is global, not per windowtreehost. Move.

  // There are up to four connections that need to be notified:
  // . the connection containing |old_focused_window|.
  // . the connection with |old_focused_window| as its root.
  // . the connection containing |new_focused_window|.
  // . the connection with |new_focused_window| as its root.
  // Some of these connections may be the same. The following takes care to
  // notify each only once.
  WindowTree* owning_tree_old = nullptr;
  WindowTree* embedded_tree_old = nullptr;

  if (old_focused_window) {
    owning_tree_old = connection_manager_->GetTreeWithId(
        old_focused_window->id().connection_id);
    if (owning_tree_old) {
      owning_tree_old->ProcessFocusChanged(old_focused_window,
                                           new_focused_window);
    }
    embedded_tree_old =
        connection_manager_->GetTreeWithRoot(old_focused_window);
    if (embedded_tree_old) {
      DCHECK_NE(owning_tree_old, embedded_tree_old);
      embedded_tree_old->ProcessFocusChanged(old_focused_window,
                                             new_focused_window);
    }
  }
  WindowTree* owning_tree_new = nullptr;
  WindowTree* embedded_tree_new = nullptr;
  if (new_focused_window) {
    owning_tree_new = connection_manager_->GetTreeWithId(
        new_focused_window->id().connection_id);
    if (owning_tree_new && owning_tree_new != owning_tree_old &&
        owning_tree_new != embedded_tree_old) {
      owning_tree_new->ProcessFocusChanged(old_focused_window,
                                           new_focused_window);
    }
    embedded_tree_new =
        connection_manager_->GetTreeWithRoot(new_focused_window);
    if (embedded_tree_new && embedded_tree_new != owning_tree_old &&
        embedded_tree_new != embedded_tree_old) {
      DCHECK_NE(owning_tree_new, embedded_tree_new);
      embedded_tree_new->ProcessFocusChanged(old_focused_window,
                                             new_focused_window);
    }
  }

  // WindowManagers are always notified of focus changes.
  WindowTree* wms_tree_with_old_focused_window = nullptr;
  if (old_focused_window) {
    WindowManagerState* wms =
        display_manager()
            ->GetWindowManagerAndDisplay(old_focused_window)
            .window_manager_state;
    wms_tree_with_old_focused_window = wms ? wms->tree() : nullptr;
    if (wms_tree_with_old_focused_window &&
        wms_tree_with_old_focused_window != owning_tree_old &&
        wms_tree_with_old_focused_window != embedded_tree_old &&
        wms_tree_with_old_focused_window != owning_tree_new &&
        wms_tree_with_old_focused_window != embedded_tree_new) {
      wms_tree_with_old_focused_window->ProcessFocusChanged(old_focused_window,
                                                            new_focused_window);
    }
  }
  if (new_focused_window) {
    WindowManagerState* wms =
        display_manager()
            ->GetWindowManagerAndDisplay(new_focused_window)
            .window_manager_state;
    WindowTree* wms_tree = wms ? wms->tree() : nullptr;
    if (wms_tree && wms_tree != wms_tree_with_old_focused_window &&
        wms_tree != owning_tree_old && wms_tree != embedded_tree_old &&
        wms_tree != owning_tree_new && wms_tree != embedded_tree_new) {
      wms_tree->ProcessFocusChanged(old_focused_window, new_focused_window);
    }
  }

  UpdateTextInputState(new_focused_window,
                       new_focused_window->text_input_state());
}

void Display::OnWindowDestroyed(ServerWindow* window) {
  windows_needing_frame_destruction_.erase(window);
  window->RemoveObserver(this);
}

void Display::OnActiveUserIdChanged(const UserId& previously_active_id,
                                    const UserId& active_id) {
  WindowManagerState* previous_wms =
      GetWindowManagerStateForUser(previously_active_id);
  const gfx::Point mouse_location_on_screen =
      previous_wms
          ? previous_wms->event_dispatcher()->mouse_pointer_last_location()
          : gfx::Point();
  if (previous_wms)
    previous_wms->event_dispatcher()->Reset();

  WindowManagerState* active_wms = GetWindowManagerStateForUser(active_id);
  if (active_wms) {
    active_wms->event_dispatcher()->Reset();
    active_wms->event_dispatcher()->SetMousePointerScreenLocation(
        mouse_location_on_screen);
  }
}

void Display::OnUserIdAdded(const UserId& id) {}

void Display::OnUserIdRemoved(const UserId& id) {
  if (binding_)
    return;

  WindowManagerState* state = GetWindowManagerStateForUser(id);
  if (!state)
    return;

  // DestroyTree() calls back to OnWillDestroyTree() and the WindowManagerState
  // is destroyed (and removed).
  connection_manager_->DestroyTree(state->tree());
  DCHECK_EQ(0u, window_manager_state_map_.count(id));
}

void Display::OnWindowManagerFactorySet(WindowManagerFactoryService* service) {
  if (!binding_)
    CreateWindowManagerStateFromService(service);
}

}  // namespace ws
}  // namespace mus
