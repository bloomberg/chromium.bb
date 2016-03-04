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
#include "components/mus/ws/focus_controller.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/window_manager_factory_service.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"

namespace mus {
namespace ws {
namespace {

base::TimeDelta GetDefaultAckTimerDelay() {
#if defined(NDEBUG)
  return base::TimeDelta::FromMilliseconds(100);
#else
  return base::TimeDelta::FromMilliseconds(1000);
#endif
}

bool EventsCanBeCoalesced(const ui::Event& one, const ui::Event& two) {
  if (one.type() != two.type() || one.flags() != two.flags())
    return false;

  // TODO(sad): wheel events can also be merged.
  if (one.type() != ui::ET_POINTER_MOVED)
    return false;

  return one.AsPointerEvent()->pointer_id() ==
         two.AsPointerEvent()->pointer_id();
}

scoped_ptr<ui::Event> CoalesceEvents(scoped_ptr<ui::Event> first,
                                     scoped_ptr<ui::Event> second) {
  DCHECK(first->type() == ui::ET_POINTER_MOVED)
      << " Non-move events cannot be merged yet.";
  // For mouse moves, the new event just replaces the old event.
  return second;
}

uint32_t next_id = 1;

}  // namespace

class Display::ProcessedEventTarget {
 public:
  ProcessedEventTarget(ServerWindow* window, bool in_nonclient_area)
      : in_nonclient_area_(in_nonclient_area) {
    tracker_.Add(window);
  }

  ~ProcessedEventTarget() {}

  // Return true if the event is still valid. The event becomes invalid if
  // the window is destroyed while waiting to dispatch.
  bool IsValid() const { return !tracker_.windows().empty(); }

  ServerWindow* window() {
    DCHECK(IsValid());
    return tracker_.windows().front();
  }

  bool in_nonclient_area() const { return in_nonclient_area_; }

 private:
  ServerWindowTracker tracker_;
  const bool in_nonclient_area_;

  DISALLOW_COPY_AND_ASSIGN(ProcessedEventTarget);
};

Display::QueuedEvent::QueuedEvent() {}
Display::QueuedEvent::~QueuedEvent() {}

Display::Display(ConnectionManager* connection_manager,
                 mojo::Connector* connector,
                 const scoped_refptr<GpuState>& gpu_state,
                 const scoped_refptr<SurfacesState>& surfaces_state)
    : id_(next_id++),
      connection_manager_(connection_manager),
      event_dispatcher_(this),
      platform_display_(
          PlatformDisplay::Create(connector, gpu_state, surfaces_state)),
      tree_awaiting_input_ack_(nullptr),
      last_cursor_(0) {
  frame_decoration_values_ = mojom::FrameDecorationValues::New();
  frame_decoration_values_->normal_client_area_insets = mojo::Insets::New();
  frame_decoration_values_->maximized_client_area_insets = mojo::Insets::New();
  frame_decoration_values_->max_title_bar_button_width = 0u;

  platform_display_->Init(this);

  connection_manager_->window_manager_factory_registry()->AddObserver(this);
}

Display::~Display() {
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
  connection_manager_->AddDisplay(this);
  InitWindowManagersIfNecessary();
}

void Display::SetFrameDecorationValues(mojom::FrameDecorationValuesPtr values) {
  // TODO(sky): this needs to be moved to WindowManagerState.
  frame_decoration_values_ = values.Clone();
  connection_manager_->ProcessFrameDecorationValuesChanged(this);
}

bool Display::SchedulePaintIfInViewport(const ServerWindow* window,
                                        const gfx::Rect& bounds) {
  if (root_->Contains(window)) {
    platform_display_->SchedulePaint(window, bounds);
    return true;
  }
  return false;
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

WindowManagerState* Display::GetWindowManagerStateForUser(UserId user_id) {
  auto iter = window_manager_state_map_.find(user_id);
  return iter == window_manager_state_map_.end() ? nullptr : iter->second.get();
}

void Display::SetCapture(ServerWindow* window, bool in_nonclient_area) {
  ServerWindow* capture_window = event_dispatcher_.capture_window();
  if (capture_window == window)
    return;
  event_dispatcher_.SetCaptureWindow(window, in_nonclient_area);
}

mojom::Rotation Display::GetRotation() const {
  return platform_display_->GetRotation();
}

void Display::SetFocusedWindow(ServerWindow* new_focused_window) {
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

void Display::OnWindowTreeConnectionError(WindowTree* tree) {
  for (auto it = window_manager_state_map_.begin();
       it != window_manager_state_map_.end(); ++it) {
    if (it->second->tree() == tree) {
      window_manager_state_map_.erase(it);
      break;
    }
  }

  if (tree_awaiting_input_ack_ != tree)
    return;
  // The WindowTree is dying. So it's not going to ack the event.
  OnEventAck(tree_awaiting_input_ack_);
}

void Display::OnCursorUpdated(ServerWindow* window) {
  if (window == event_dispatcher_.mouse_cursor_source_window())
    UpdateNativeCursor(window->cursor());
}

void Display::MaybeChangeCursorOnWindowTreeChange() {
  event_dispatcher_.UpdateCursorProviderByLastKnownLocation();
  ServerWindow* cursor_source_window =
      event_dispatcher_.mouse_cursor_source_window();
  if (cursor_source_window)
    UpdateNativeCursor(cursor_source_window->cursor());
}

void Display::SetSize(mojo::SizePtr size) {
  platform_display_->SetViewportSize(size.To<gfx::Size>());
}

void Display::SetTitle(const mojo::String& title) {
  platform_display_->SetTitle(title.To<base::string16>());
}

void Display::OnEventAck(mojom::WindowTree* tree) {
  if (tree_awaiting_input_ack_ != tree) {
    // TODO(sad): The ack must have arrived after the timeout. We should do
    // something here, and in OnEventAckTimeout().
    return;
  }
  tree_awaiting_input_ack_ = nullptr;
  event_ack_timer_.Stop();
  ProcessNextEventFromQueue();
}

void Display::InitWindowManagersIfNecessary() {
  if (!init_called_ || !root_)
    return;

  connection_manager_->OnDisplayAcceleratedWidgetAvailable(this);
  if (binding_) {
    scoped_ptr<WindowManagerState> wms_ptr(new WindowManagerState(this));
    WindowManagerState* wms = wms_ptr.get();
    // For this case we never create additional WindowManagerStates, so any
    // id works.
    window_manager_state_map_[0u] = std::move(wms_ptr);
    wms->tree_ = binding_->CreateWindowTree(wms->root());
  } else {
    CreateWindowManagerStatesFromRegistry();
  }
}

void Display::OnEventAckTimeout() {
  // TODO(sad): Figure out what we should do.
  NOTIMPLEMENTED() << "Event ACK timed out.";
  OnEventAck(tree_awaiting_input_ack_);
}

void Display::QueueEvent(
    const ui::Event& event,
    scoped_ptr<ProcessedEventTarget> processed_event_target) {
  scoped_ptr<QueuedEvent> queued_event(new QueuedEvent);
  queued_event->event = ui::Event::Clone(event);
  queued_event->processed_target = std::move(processed_event_target);
  event_queue_.push(std::move(queued_event));
}

void Display::ProcessNextEventFromQueue() {
  // Loop through |event_queue_| stopping after dispatching the first valid
  // event.
  while (!event_queue_.empty()) {
    scoped_ptr<QueuedEvent> queued_event = std::move(event_queue_.front());
    event_queue_.pop();
    if (!queued_event->processed_target) {
      event_dispatcher_.ProcessEvent(*queued_event->event);
      return;
    }
    if (queued_event->processed_target->IsValid()) {
      DispatchInputEventToWindowImpl(
          queued_event->processed_target->window(),
          queued_event->processed_target->in_nonclient_area(),
          *queued_event->event);
      return;
    }
  }
}

void Display::DispatchInputEventToWindowImpl(ServerWindow* target,
                                             bool in_nonclient_area,
                                             const ui::Event& event) {
  if (target == root_.get()) {
    // TODO(sky): use active windowmanager here.
    target = GetFirstWindowManagerState()->root();
  }
  if (event.IsMousePointerEvent()) {
    DCHECK(event_dispatcher_.mouse_cursor_source_window());
    UpdateNativeCursor(
        event_dispatcher_.mouse_cursor_source_window()->cursor());
  }

  // If the event is in the non-client area the event goes to the owner of
  // the window. Otherwise if the window is an embed root, forward to the
  // embedded window.
  WindowTree* tree =
      in_nonclient_area
          ? connection_manager_->GetTreeWithId(target->id().connection_id)
          : connection_manager_->GetTreeWithRoot(target);
  if (!tree) {
    DCHECK(!in_nonclient_area);
    tree = connection_manager_->GetTreeWithId(target->id().connection_id);
  }

  // TOOD(sad): Adjust this delay, possibly make this dynamic.
  const base::TimeDelta max_delay = base::debug::BeingDebugged()
                                        ? base::TimeDelta::FromDays(1)
                                        : GetDefaultAckTimerDelay();
  event_ack_timer_.Start(FROM_HERE, max_delay, this,
                         &Display::OnEventAckTimeout);

  tree_awaiting_input_ack_ = tree;
  tree->DispatchInputEvent(target, mojom::Event::From(event));
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
      new WindowManagerState(this, service->user_id()));
  WindowManagerState* wms = wms_ptr.get();
  window_manager_state_map_[service->user_id()] = std::move(wms_ptr);
  wms->tree_ = connection_manager_->CreateTreeForWindowManager(
      this, service->window_manager_factory(), wms->root());
}

void Display::UpdateNativeCursor(int32_t cursor_id) {
  if (cursor_id != last_cursor_) {
    platform_display_->SetCursorById(cursor_id);
    last_cursor_ = cursor_id;
  }
}

ServerWindow* Display::GetRootWindow() {
  return root_.get();
}

void Display::OnEvent(const ui::Event& event) {
  mojom::EventPtr mojo_event(mojom::Event::From(event));
  // If this is still waiting for an ack from a previously sent event, then
  // queue up the event to be dispatched once the ack is received.
  if (event_ack_timer_.IsRunning()) {
    if (!event_queue_.empty() && !event_queue_.back()->processed_target &&
        EventsCanBeCoalesced(*event_queue_.back()->event, event)) {
      event_queue_.back()->event = CoalesceEvents(
          std::move(event_queue_.back()->event), ui::Event::Clone(event));
      return;
    }
    QueueEvent(event, nullptr);
    return;
  }
  event_dispatcher_.ProcessEvent(event);
}

void Display::OnNativeCaptureLost() {
  SetCapture(nullptr, false);
}

void Display::OnDisplayClosed() {
  connection_manager_->DestroyDisplay(this);
}

void Display::OnViewportMetricsChanged(
    const mojom::ViewportMetrics& old_metrics,
    const mojom::ViewportMetrics& new_metrics) {
  if (!root_) {
    root_.reset(connection_manager_->CreateServerWindow(
        RootWindowId(connection_manager_->GetAndAdvanceNextRootId()),
        ServerWindow::Properties()));
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
    root_->SetVisible(true);
    focus_controller_.reset(new FocusController(this, root_.get()));
    focus_controller_->AddObserver(this);
    InitWindowManagersIfNecessary();
    event_dispatcher_.set_root(root_.get());
  } else {
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
    const gfx::Rect wm_bounds(root_->bounds().size());
    for (auto& pair : window_manager_state_map_)
      pair.second->root()->SetBounds(wm_bounds);
  }
  connection_manager_->ProcessViewportMetricsChanged(this, old_metrics,
                                                     new_metrics);
}

void Display::OnTopLevelSurfaceChanged(cc::SurfaceId surface_id) {
  event_dispatcher_.set_surface_id(surface_id);
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
        connection_manager_->GetWindowManagerAndDisplay(old_focused_window)
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
        connection_manager_->GetWindowManagerAndDisplay(new_focused_window)
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

void Display::OnAccelerator(uint32_t accelerator_id, const ui::Event& event) {
  // TODO(sky): accelerators need to be maintained per windowmanager and pushed
  // to the eventdispatcher when the active userid changes.
  GetFirstWindowManagerState()->tree()->OnAccelerator(
      accelerator_id, mojom::Event::From(event));
}

void Display::SetFocusedWindowFromEventDispatcher(
    ServerWindow* new_focused_window) {
  SetFocusedWindow(new_focused_window);
}

ServerWindow* Display::GetFocusedWindowForEventDispatcher() {
  return GetFocusedWindow();
}

void Display::SetNativeCapture() {
  platform_display_->SetCapture();
}

void Display::ReleaseNativeCapture() {
  platform_display_->ReleaseCapture();
}

void Display::OnServerWindowCaptureLost(ServerWindow* window) {
  DCHECK(window);
  connection_manager_->ProcessLostCapture(window);
}

void Display::DispatchInputEventToWindow(ServerWindow* target,
                                         bool in_nonclient_area,
                                         const ui::Event& event) {
  if (event_ack_timer_.IsRunning()) {
    scoped_ptr<ProcessedEventTarget> processed_event_target(
        new ProcessedEventTarget(target, in_nonclient_area));
    QueueEvent(event, std::move(processed_event_target));
    return;
  }

  DispatchInputEventToWindowImpl(target, in_nonclient_area, event);
}

void Display::OnWindowDestroyed(ServerWindow* window) {
  windows_needing_frame_destruction_.erase(window);
  window->RemoveObserver(this);
}

void Display::OnActiveUserIdChanged(UserId id) {
  // TODO(sky): this likely needs to cancel any pending events and all that.
}

void Display::OnUserIdAdded(UserId id) {}

void Display::OnUserIdRemoved(UserId id) {
  if (binding_)
    return;

  WindowManagerState* state = GetWindowManagerStateForUser(id);
  if (!state)
    return;

  // DestroyTree() calls back to OnWindowTreeConnectionError() and the
  // WindowManagerState is destroyed (and removed).
  connection_manager_->DestroyTree(state->tree());
  DCHECK_EQ(0u, window_manager_state_map_.count(id));
}

void Display::OnWindowManagerFactorySet(WindowManagerFactoryService* service) {
  CreateWindowManagerStateFromService(service);
}

}  // namespace ws
}  // namespace mus
