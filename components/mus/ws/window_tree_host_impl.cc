// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_host_impl.h"

#include "base/debug/debugger.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mus/common/types.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/focus_controller.h"
#include "components/mus/ws/window_manager_factory_service.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_tree_host_connection.h"
#include "components/mus/ws/window_tree_impl.h"
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

class WindowTreeHostImpl::ProcessedEventTarget {
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

WindowTreeHostImpl::QueuedEvent::QueuedEvent() {}
WindowTreeHostImpl::QueuedEvent::~QueuedEvent() {}

WindowTreeHostImpl::WindowTreeHostImpl(
    ConnectionManager* connection_manager,
    mojo::Connector* connector,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<SurfacesState>& surfaces_state)
    : id_(next_id++),
      connection_manager_(connection_manager),
      event_dispatcher_(this),
      display_manager_(
          DisplayManager::Create(connector, gpu_state, surfaces_state)),
      tree_awaiting_input_ack_(nullptr),
      last_cursor_(0) {
  frame_decoration_values_ = mojom::FrameDecorationValues::New();
  frame_decoration_values_->normal_client_area_insets = mojo::Insets::New();
  frame_decoration_values_->maximized_client_area_insets = mojo::Insets::New();
  frame_decoration_values_->max_title_bar_button_width = 0u;

  display_manager_->Init(this);
}

WindowTreeHostImpl::~WindowTreeHostImpl() {
  DestroyFocusController();
  for (ServerWindow* window : windows_needing_frame_destruction_)
    window->RemoveObserver(this);

  // TODO(sky): this may leave the WindowTreeImpls associated with the
  // WindowManagerStates still alive. The shutdown ordering is a bit iffy,
  // figure out what it should be. ConnectionManager::OnConnectionError()
  // has a check that effects this too.
}

void WindowTreeHostImpl::Init(scoped_ptr<WindowTreeHostConnection> connection) {
  init_called_ = true;
  window_tree_host_connection_ = std::move(connection);
  connection_manager_->AddHost(this);
  InitWindowManagersIfNecessary();
}

void WindowTreeHostImpl::SetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  // TODO(sky): this needs to be moved to WindowManagerState.
  frame_decoration_values_ = values.Clone();
  connection_manager_->ProcessFrameDecorationValuesChanged(this);
}

bool WindowTreeHostImpl::SchedulePaintIfInViewport(const ServerWindow* window,
                                                   const gfx::Rect& bounds) {
  if (root_->Contains(window)) {
    display_manager_->SchedulePaint(window, bounds);
    return true;
  }
  return false;
}

void WindowTreeHostImpl::ScheduleSurfaceDestruction(ServerWindow* window) {
  if (!display_manager_->IsFramePending()) {
    window->DestroySurfacesScheduledForDestruction();
    return;
  }
  if (windows_needing_frame_destruction_.count(window))
    return;
  windows_needing_frame_destruction_.insert(window);
  window->AddObserver(this);
}

const mojom::ViewportMetrics& WindowTreeHostImpl::GetViewportMetrics() const {
  return display_manager_->GetViewportMetrics();
}

ServerWindow* WindowTreeHostImpl::GetRootWithId(const WindowId& id) {
  if (id == root_->id())
    return root_.get();
  for (auto& pair : window_manager_state_map_) {
    if (pair.second->root()->id() == id)
      return pair.second->root();
  }
  return nullptr;
}

WindowManagerState* WindowTreeHostImpl::GetWindowManagerStateWithRoot(
    const ServerWindow* window) {
  for (auto& pair : window_manager_state_map_) {
    if (pair.second->root() == window)
      return pair.second.get();
  }
  return nullptr;
}

WindowManagerState* WindowTreeHostImpl::GetFirstWindowManagerState() {
  return window_manager_state_map_.empty()
             ? nullptr
             : window_manager_state_map_.begin()->second.get();
}

void WindowTreeHostImpl::SetCapture(ServerWindow* window,
                                    bool in_nonclient_area) {
  ServerWindow* capture_window = event_dispatcher_.capture_window();
  if (capture_window == window)
    return;
  event_dispatcher_.SetCaptureWindow(window, in_nonclient_area);
}

mojom::Rotation WindowTreeHostImpl::GetRotation() const {
  return display_manager_->GetRotation();
}

void WindowTreeHostImpl::SetFocusedWindow(ServerWindow* new_focused_window) {
  ServerWindow* old_focused_window = focus_controller_->GetFocusedWindow();
  if (old_focused_window == new_focused_window)
    return;
  DCHECK(root_window()->Contains(new_focused_window));
  focus_controller_->SetFocusedWindow(new_focused_window);
}

ServerWindow* WindowTreeHostImpl::GetFocusedWindow() {
  return focus_controller_->GetFocusedWindow();
}

void WindowTreeHostImpl::DestroyFocusController() {
  if (!focus_controller_)
    return;

  focus_controller_->RemoveObserver(this);
  focus_controller_.reset();
}

void WindowTreeHostImpl::AddActivationParent(ServerWindow* window) {
  activation_parents_.Add(window);
}

void WindowTreeHostImpl::RemoveActivationParent(ServerWindow* window) {
  activation_parents_.Remove(window);
}

void WindowTreeHostImpl::UpdateTextInputState(ServerWindow* window,
                                              const ui::TextInputState& state) {
  // Do not need to update text input for unfocused windows.
  if (!display_manager_ || focus_controller_->GetFocusedWindow() != window)
    return;
  display_manager_->UpdateTextInputState(state);
}

void WindowTreeHostImpl::SetImeVisibility(ServerWindow* window, bool visible) {
  // Do not need to show or hide IME for unfocused window.
  if (focus_controller_->GetFocusedWindow() != window)
    return;
  display_manager_->SetImeVisibility(visible);
}

void WindowTreeHostImpl::OnWindowTreeConnectionError(WindowTreeImpl* tree) {
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

void WindowTreeHostImpl::OnCursorUpdated(ServerWindow* window) {
  if (window == event_dispatcher_.mouse_cursor_source_window())
    UpdateNativeCursor(window->cursor());
}

void WindowTreeHostImpl::MaybeChangeCursorOnWindowTreeChange() {
  event_dispatcher_.UpdateCursorProviderByLastKnownLocation();
  ServerWindow* cursor_source_window =
      event_dispatcher_.mouse_cursor_source_window();
  if (cursor_source_window)
    UpdateNativeCursor(cursor_source_window->cursor());
}

void WindowTreeHostImpl::SetSize(mojo::SizePtr size) {
  display_manager_->SetViewportSize(size.To<gfx::Size>());
}

void WindowTreeHostImpl::SetTitle(const mojo::String& title) {
  display_manager_->SetTitle(title.To<base::string16>());
}

void WindowTreeHostImpl::OnEventAck(mojom::WindowTree* tree) {
  if (tree_awaiting_input_ack_ != tree) {
    // TODO(sad): The ack must have arrived after the timeout. We should do
    // something here, and in OnEventAckTimeout().
    return;
  }
  tree_awaiting_input_ack_ = nullptr;
  event_ack_timer_.Stop();
  ProcessNextEventFromQueue();
}

void WindowTreeHostImpl::InitWindowManagersIfNecessary() {
  if (!init_called_ || !root_)
    return;

  connection_manager_->OnWindowTreeHostDisplayAvailable(this);
  if (window_tree_host_connection_) {
    scoped_ptr<WindowManagerState> wms_ptr(new WindowManagerState(this));
    WindowManagerState* wms = wms_ptr.get();
    // For this case we never create additional WindowManagerStates, so any
    // id works.
    window_manager_state_map_[0u] = std::move(wms_ptr);
    wms->tree_ = window_tree_host_connection_->CreateWindowTree(wms->root());
  } else {
    CreateWindowManagerStatesFromRegistry();
  }
}

void WindowTreeHostImpl::OnEventAckTimeout() {
  // TODO(sad): Figure out what we should do.
  NOTIMPLEMENTED() << "Event ACK timed out.";
  OnEventAck(tree_awaiting_input_ack_);
}

void WindowTreeHostImpl::QueueEvent(
    const ui::Event& event,
    scoped_ptr<ProcessedEventTarget> processed_event_target) {
  scoped_ptr<QueuedEvent> queued_event(new QueuedEvent);
  queued_event->event = ui::Event::Clone(event);
  queued_event->processed_target = std::move(processed_event_target);
  event_queue_.push(std::move(queued_event));
}

void WindowTreeHostImpl::ProcessNextEventFromQueue() {
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

void WindowTreeHostImpl::DispatchInputEventToWindowImpl(
    ServerWindow* target,
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
  WindowTreeImpl* connection =
      in_nonclient_area
          ? connection_manager_->GetConnection(target->id().connection_id)
          : connection_manager_->GetConnectionWithRoot(target);
  if (!connection) {
    DCHECK(!in_nonclient_area);
    connection = connection_manager_->GetConnection(target->id().connection_id);
  }

  // TOOD(sad): Adjust this delay, possibly make this dynamic.
  const base::TimeDelta max_delay = base::debug::BeingDebugged()
                                        ? base::TimeDelta::FromDays(1)
                                        : GetDefaultAckTimerDelay();
  event_ack_timer_.Start(FROM_HERE, max_delay, this,
                         &WindowTreeHostImpl::OnEventAckTimeout);

  tree_awaiting_input_ack_ = connection;
  connection->DispatchInputEvent(target, mojom::Event::From(event));
}

void WindowTreeHostImpl::CreateWindowManagerStatesFromRegistry() {
  std::vector<WindowManagerFactoryService*> services =
      connection_manager_->window_manager_factory_registry()->GetServices();
  for (WindowManagerFactoryService* service : services) {
    scoped_ptr<WindowManagerState> wms_ptr(
        new WindowManagerState(this, service->user_id()));
    WindowManagerState* wms = wms_ptr.get();
    window_manager_state_map_[service->user_id()] = std::move(wms_ptr);
    wms->tree_ = connection_manager_->CreateTreeForWindowManager(
        this, service->window_manager_factory(), wms->root());
  }
}

void WindowTreeHostImpl::UpdateNativeCursor(int32_t cursor_id) {
  if (cursor_id != last_cursor_) {
    display_manager_->SetCursorById(cursor_id);
    last_cursor_ = cursor_id;
  }
}

ServerWindow* WindowTreeHostImpl::GetRootWindow() {
  return root_.get();
}

void WindowTreeHostImpl::OnEvent(const ui::Event& event) {
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

void WindowTreeHostImpl::OnNativeCaptureLost() {
  SetCapture(nullptr, false);
}

void WindowTreeHostImpl::OnDisplayClosed() {
  connection_manager_->DestroyHost(this);
}

void WindowTreeHostImpl::OnViewportMetricsChanged(
    const mojom::ViewportMetrics& old_metrics,
    const mojom::ViewportMetrics& new_metrics) {
  if (!root_) {
    root_.reset(connection_manager_->CreateServerWindow(
        RootWindowId(connection_manager_->GetAndAdvanceNextHostId()),
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

void WindowTreeHostImpl::OnTopLevelSurfaceChanged(cc::SurfaceId surface_id) {
  event_dispatcher_.set_surface_id(surface_id);
}

void WindowTreeHostImpl::OnCompositorFrameDrawn() {
  std::set<ServerWindow*> windows;
  windows.swap(windows_needing_frame_destruction_);
  for (ServerWindow* window : windows) {
    window->RemoveObserver(this);
    window->DestroySurfacesScheduledForDestruction();
  }
}

bool WindowTreeHostImpl::CanHaveActiveChildren(ServerWindow* window) const {
  return window && activation_parents_.Contains(window);
}

void WindowTreeHostImpl::OnActivationChanged(ServerWindow* old_active_window,
                                             ServerWindow* new_active_window) {
  DCHECK_NE(new_active_window, old_active_window);
  if (new_active_window && new_active_window->parent()) {
    // Raise the new active window.
    // TODO(sad): Let the WM dictate whether to raise the window or not?
    new_active_window->parent()->StackChildAtTop(new_active_window);
  }
}

void WindowTreeHostImpl::OnFocusChanged(
    FocusControllerChangeSource change_source,
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
  WindowTreeImpl* owning_connection_old = nullptr;
  WindowTreeImpl* embedded_connection_old = nullptr;

  if (old_focused_window) {
    owning_connection_old = connection_manager_->GetConnection(
        old_focused_window->id().connection_id);
    if (owning_connection_old) {
      owning_connection_old->ProcessFocusChanged(old_focused_window,
                                                 new_focused_window);
    }
    embedded_connection_old =
        connection_manager_->GetConnectionWithRoot(old_focused_window);
    if (embedded_connection_old) {
      DCHECK_NE(owning_connection_old, embedded_connection_old);
      embedded_connection_old->ProcessFocusChanged(old_focused_window,
                                                   new_focused_window);
    }
  }
  WindowTreeImpl* owning_connection_new = nullptr;
  WindowTreeImpl* embedded_connection_new = nullptr;
  if (new_focused_window) {
    owning_connection_new = connection_manager_->GetConnection(
        new_focused_window->id().connection_id);
    if (owning_connection_new &&
        owning_connection_new != owning_connection_old &&
        owning_connection_new != embedded_connection_old) {
      owning_connection_new->ProcessFocusChanged(old_focused_window,
                                                 new_focused_window);
    }
    embedded_connection_new =
        connection_manager_->GetConnectionWithRoot(new_focused_window);
    if (embedded_connection_new &&
        embedded_connection_new != owning_connection_old &&
        embedded_connection_new != embedded_connection_old) {
      DCHECK_NE(owning_connection_new, embedded_connection_new);
      embedded_connection_new->ProcessFocusChanged(old_focused_window,
                                                   new_focused_window);
    }
  }

  // WindowManagers are always notified of focus changes.
  WindowTreeImpl* wms_tree_with_old_focused_window = nullptr;
  if (old_focused_window) {
    WindowManagerState* wms =
        connection_manager_->GetWindowManagerAndHost(old_focused_window)
            .window_manager_state;
    wms_tree_with_old_focused_window = wms ? wms->tree() : nullptr;
    if (wms_tree_with_old_focused_window &&
        wms_tree_with_old_focused_window != owning_connection_old &&
        wms_tree_with_old_focused_window != embedded_connection_old &&
        wms_tree_with_old_focused_window != owning_connection_new &&
        wms_tree_with_old_focused_window != embedded_connection_new) {
      wms_tree_with_old_focused_window->ProcessFocusChanged(old_focused_window,
                                                            new_focused_window);
    }
  }
  if (new_focused_window) {
    WindowManagerState* wms =
        connection_manager_->GetWindowManagerAndHost(new_focused_window)
            .window_manager_state;
    WindowTreeImpl* wms_tree = wms ? wms->tree() : nullptr;
    if (wms_tree && wms_tree != wms_tree_with_old_focused_window &&
        wms_tree != owning_connection_old &&
        wms_tree != embedded_connection_old &&
        wms_tree != owning_connection_new &&
        wms_tree != embedded_connection_new) {
      wms_tree->ProcessFocusChanged(old_focused_window, new_focused_window);
    }
  }

  UpdateTextInputState(new_focused_window,
                       new_focused_window->text_input_state());
}

void WindowTreeHostImpl::OnAccelerator(uint32_t accelerator_id,
                                       const ui::Event& event) {
  // TODO(sky): accelerators need to be maintained per windowmanager and pushed
  // to the eventdispatcher when the active userid changes.
  GetFirstWindowManagerState()->tree()->OnAccelerator(
      accelerator_id, mojom::Event::From(event));
}

void WindowTreeHostImpl::SetFocusedWindowFromEventDispatcher(
    ServerWindow* new_focused_window) {
  SetFocusedWindow(new_focused_window);
}

ServerWindow* WindowTreeHostImpl::GetFocusedWindowForEventDispatcher() {
  return GetFocusedWindow();
}

void WindowTreeHostImpl::SetNativeCapture() {
  display_manager_->SetCapture();
}

void WindowTreeHostImpl::ReleaseNativeCapture() {
  display_manager_->ReleaseCapture();
}

void WindowTreeHostImpl::OnServerWindowCaptureLost(ServerWindow* window) {
  DCHECK(window);
  connection_manager_->ProcessLostCapture(window);
}

void WindowTreeHostImpl::DispatchInputEventToWindow(ServerWindow* target,
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

void WindowTreeHostImpl::OnWindowDestroyed(ServerWindow* window) {
  windows_needing_frame_destruction_.erase(window);
  window->RemoveObserver(this);
}

}  // namespace ws
}  // namespace mus
