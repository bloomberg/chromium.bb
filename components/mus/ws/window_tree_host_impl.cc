// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_host_impl.h"

#include "base/debug/debugger.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/input_event_constants.mojom.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/focus_controller.h"
#include "components/mus/ws/window_tree_host_delegate.h"
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

bool EventsCanBeCoalesced(const mojom::Event& one, const mojom::Event& two) {
  if (one.action != two.action || one.flags != two.flags)
    return false;
  // TODO(sad): wheel events can also be merged.
  if (one.action != mojom::EventType::POINTER_MOVE)
    return false;
  DCHECK(one.pointer_data);
  DCHECK(two.pointer_data);
  if (one.pointer_data->kind != two.pointer_data->kind ||
      one.pointer_data->pointer_id != two.pointer_data->pointer_id)
    return false;

  return true;
}

mojom::EventPtr CoalesceEvents(mojom::EventPtr first, mojom::EventPtr second) {
  DCHECK_EQ(first->action, mojom::EventType::POINTER_MOVE)
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
    mojom::WindowTreeHostClientPtr client,
    ConnectionManager* connection_manager,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<SurfacesState>& surfaces_state,
    mojom::WindowManagerPtr window_manager)
    : id_(next_id++),
      delegate_(nullptr),
      connection_manager_(connection_manager),
      client_(std::move(client)),
      event_dispatcher_(this),
      display_manager_(
          DisplayManager::Create(app_impl, gpu_state, surfaces_state)),
      window_manager_(std::move(window_manager)),
      tree_awaiting_input_ack_(nullptr),
      last_cursor_(0) {
  frame_decoration_values_ = mojom::FrameDecorationValues::New();
  frame_decoration_values_->normal_client_area_insets = mojo::Insets::New();
  frame_decoration_values_->maximized_client_area_insets = mojo::Insets::New();
  frame_decoration_values_->max_title_bar_button_width = 0u;

  display_manager_->Init(this);
  if (client_) {
    client_.set_connection_error_handler(base::Bind(
        &WindowTreeHostImpl::OnClientClosed, base::Unretained(this)));
  }
}

WindowTreeHostImpl::~WindowTreeHostImpl() {
  DestroyFocusController();
  for (ServerWindow* window : windows_needing_frame_destruction_)
    window->RemoveObserver(this);
}

void WindowTreeHostImpl::Init(WindowTreeHostDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_ && root_)
    delegate_->OnDisplayInitialized();
}

const WindowTreeImpl* WindowTreeHostImpl::GetWindowTree() const {
  return delegate_ ? delegate_->GetWindowTree() : nullptr;
}

WindowTreeImpl* WindowTreeHostImpl::GetWindowTree() {
  return delegate_ ? delegate_->GetWindowTree() : nullptr;
}

void WindowTreeHostImpl::SetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  frame_decoration_values_ = values.Clone();
  connection_manager_->ProcessFrameDecorationValuesChanged(this);
}

bool WindowTreeHostImpl::IsWindowAttachedToRoot(
    const ServerWindow* window) const {
  return root_->Contains(window) && window != root_.get();
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

void WindowTreeHostImpl::AddAccelerator(
    uint32_t id,
    mojom::EventMatcherPtr event_matcher,
    const AddAcceleratorCallback& callback) {
  bool success = event_dispatcher_.AddAccelerator(id, std::move(event_matcher));
  callback.Run(success);
}

void WindowTreeHostImpl::RemoveAccelerator(uint32_t id) {
  event_dispatcher_.RemoveAccelerator(id);
}

void WindowTreeHostImpl::AddActivationParent(Id transport_window_id) {
  ServerWindow* window = GetWindowFromWindowTreeHost(transport_window_id);
  if (window)
    activation_parents_.insert(window->id());
}

void WindowTreeHostImpl::RemoveActivationParent(Id transport_window_id) {
  ServerWindow* window = GetWindowFromWindowTreeHost(transport_window_id);
  if (window)
    activation_parents_.erase(window->id());
}

void WindowTreeHostImpl::ActivateNextWindow() {
  focus_controller_->ActivateNextWindow();
}

void WindowTreeHostImpl::SetUnderlaySurfaceOffsetAndExtendedHitArea(
    Id window_id,
    int32_t x_offset,
    int32_t y_offset,
    mojo::InsetsPtr hit_area) {
  ServerWindow* window = GetWindowFromWindowTreeHost(window_id);
  if (!window)
    return;

  window->SetUnderlayOffset(gfx::Vector2d(x_offset, y_offset));
  window->set_extended_hit_test_region(hit_area.To<gfx::Insets>());
}

ServerWindow* WindowTreeHostImpl::GetWindowFromWindowTreeHost(
    Id transport_window_id) {
  WindowTreeImpl* connection = GetWindowTree();
  if (!connection)
    return nullptr;
  return connection->GetWindowByClientId(ClientWindowId(transport_window_id));
}

void WindowTreeHostImpl::OnClientClosed() {
  // |display_manager_.reset()| destroys the display-manager first, and then
  // sets |display_manager_| to nullptr. However, destroying |display_manager_|
  // can destroy the corresponding WindowTreeHostConnection, and |this|. So
  // setting it to nullptr afterwards in reset() ends up writing on free'd
  // memory. So transfer over to a local scoped_ptr<> before destroying it.
  scoped_ptr<DisplayManager> temp = std::move(display_manager_);
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

void WindowTreeHostImpl::OnEventAckTimeout() {
  // TODO(sad): Figure out what we should do.
  NOTIMPLEMENTED() << "Event ACK timed out.";
  OnEventAck(tree_awaiting_input_ack_);
}

void WindowTreeHostImpl::QueueEvent(
    mojom::EventPtr event,
    scoped_ptr<ProcessedEventTarget> processed_event_target) {
  scoped_ptr<QueuedEvent> queued_event(new QueuedEvent);
  queued_event->event = std::move(event);
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
      event_dispatcher_.ProcessEvent(std::move(queued_event->event));
      return;
    }
    if (queued_event->processed_target->IsValid()) {
      DispatchInputEventToWindowImpl(
          queued_event->processed_target->window(),
          queued_event->processed_target->in_nonclient_area(),
          std::move(queued_event->event));
      return;
    }
  }
}

void WindowTreeHostImpl::DispatchInputEventToWindowImpl(ServerWindow* target,
                                                        bool in_nonclient_area,
                                                        mojom::EventPtr event) {
  if (event->pointer_data &&
      event->pointer_data->kind == mojom::PointerKind::MOUSE) {
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
  connection->DispatchInputEvent(target, std::move(event));
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
        EventsCanBeCoalesced(*event_queue_.back()->event, *mojo_event)) {
      event_queue_.back()->event = CoalesceEvents(
          std::move(event_queue_.back()->event), std::move(mojo_event));
      return;
    }
    QueueEvent(std::move(mojo_event), nullptr);
    return;
  }
  event_dispatcher_.ProcessEvent(std::move(mojo_event));
}

void WindowTreeHostImpl::OnDisplayClosed() {
  if (delegate_)
    delegate_->OnDisplayClosed();
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
    if (delegate_)
      delegate_->OnDisplayInitialized();
    event_dispatcher_.set_root(root_.get());
  } else {
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
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
  return window && activation_parents_.count(window->id()) > 0;
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

  // Ensure that we always notify the root connection of a focus change.
  WindowTreeImpl* root_tree = GetWindowTree();
  if (root_tree != owning_connection_old &&
      root_tree != embedded_connection_old &&
      root_tree != owning_connection_new &&
      root_tree != embedded_connection_new) {
    root_tree->ProcessFocusChanged(old_focused_window, new_focused_window);
  }

  UpdateTextInputState(new_focused_window,
                       new_focused_window->text_input_state());
}

void WindowTreeHostImpl::OnAccelerator(uint32_t accelerator_id,
                                       mojom::EventPtr event) {
  client()->OnAccelerator(accelerator_id, std::move(event));
}

void WindowTreeHostImpl::SetFocusedWindowFromEventDispatcher(
    ServerWindow* new_focused_window) {
  SetFocusedWindow(new_focused_window);
}

ServerWindow* WindowTreeHostImpl::GetFocusedWindowForEventDispatcher() {
  return GetFocusedWindow();
}

void WindowTreeHostImpl::DispatchInputEventToWindow(ServerWindow* target,
                                                    bool in_nonclient_area,
                                                    mojom::EventPtr event) {
  if (event_ack_timer_.IsRunning()) {
    scoped_ptr<ProcessedEventTarget> processed_event_target(
        new ProcessedEventTarget(target, in_nonclient_area));
    QueueEvent(std::move(event), std::move(processed_event_target));
    return;
  }

  DispatchInputEventToWindowImpl(target, in_nonclient_area, std::move(event));
}

void WindowTreeHostImpl::OnWindowDestroyed(ServerWindow* window) {
  windows_needing_frame_destruction_.erase(window);
  window->RemoveObserver(this);
}

}  // namespace ws
}  // namespace mus
