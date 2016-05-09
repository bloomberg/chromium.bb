// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_state.h"

#include "base/memory/weak_ptr.h"
#include "components/mus/common/event_matcher_util.h"
#include "components/mus/ws/accelerator.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/user_display_manager.h"
#include "components/mus/ws/user_id_tracker.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_tree.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "ui/events/event.h"

namespace mus {
namespace ws {
namespace {

// Debug accelerator IDs start far above the highest valid Windows command ID
// (0xDFFF) and Chrome's highest IDC command ID.
const uint32_t kPrintWindowsDebugAcceleratorId = 1u << 31;

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

std::unique_ptr<ui::Event> CoalesceEvents(std::unique_ptr<ui::Event> first,
                                          std::unique_ptr<ui::Event> second) {
  DCHECK(first->type() == ui::ET_POINTER_MOVED)
      << " Non-move events cannot be merged yet.";
  // For mouse moves, the new event just replaces the old event.
  return second;
}

}  // namespace

class WindowManagerState::ProcessedEventTarget {
 public:
  ProcessedEventTarget(ServerWindow* window,
                       bool in_nonclient_area,
                       Accelerator* accelerator)
      : in_nonclient_area_(in_nonclient_area) {
    tracker_.Add(window);
    if (accelerator)
      accelerator_ = accelerator->GetWeakPtr();
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

  base::WeakPtr<Accelerator> accelerator() { return accelerator_; }

 private:
  ServerWindowTracker tracker_;
  const bool in_nonclient_area_;
  base::WeakPtr<Accelerator> accelerator_;

  DISALLOW_COPY_AND_ASSIGN(ProcessedEventTarget);
};

WindowManagerState::QueuedEvent::QueuedEvent() {}
WindowManagerState::QueuedEvent::~QueuedEvent() {}

WindowManagerState::WindowManagerState(Display* display,
                                       PlatformDisplay* platform_display,
                                       cc::SurfaceId surface_id)
    : WindowManagerState(display,
                         platform_display,
                         surface_id,
                         false,
                         shell::mojom::kRootUserID) {}

WindowManagerState::WindowManagerState(Display* display,
                                       PlatformDisplay* platform_display,
                                       cc::SurfaceId surface_id,
                                       const UserId& user_id)
    : WindowManagerState(display, platform_display, surface_id, true, user_id) {
}

WindowManagerState::~WindowManagerState() {}

void WindowManagerState::SetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  got_frame_decoration_values_ = true;
  frame_decoration_values_ = values.Clone();
  display_->display_manager()
      ->GetUserDisplayManager(user_id_)
      ->OnFrameDecorationValuesChanged(this);
}

bool WindowManagerState::SetCapture(ServerWindow* window,
                                    bool in_nonclient_area) {
  // TODO(sky): capture should be a singleton. Need to route to WindowServer
  // so that all other EventDispatchers are updated.
  DCHECK(IsActive());
  if (capture_window() == window)
    return true;
  DCHECK(!window || root_->Contains(window));
  return event_dispatcher_.SetCaptureWindow(window, in_nonclient_area);
}

void WindowManagerState::ReleaseCaptureBlockedByModalWindow(
    const ServerWindow* modal_window) {
  event_dispatcher_.ReleaseCaptureBlockedByModalWindow(modal_window);
}

void WindowManagerState::ReleaseCaptureBlockedByAnyModalWindow() {
  event_dispatcher_.ReleaseCaptureBlockedByAnyModalWindow();
}

void WindowManagerState::AddSystemModalWindow(ServerWindow* window) {
  DCHECK(!window->transient_parent());
  event_dispatcher_.AddSystemModalWindow(window);
}

mojom::DisplayPtr WindowManagerState::ToMojomDisplay() const {
  mojom::DisplayPtr display_ptr = display_->ToMojomDisplay();
  // TODO(sky): set work area.
  display_ptr->work_area = display_ptr->bounds.Clone();
  display_ptr->frame_decoration_values = frame_decoration_values_.Clone();
  return display_ptr;
}

void WindowManagerState::OnWillDestroyTree(WindowTree* tree) {
  if (tree_awaiting_input_ack_ != tree)
    return;
  // The WindowTree is dying. So it's not going to ack the event.
  // If the dying tree matches the root |tree_| marked as handled so we don't
  // notify it of accelerators.
  OnEventAck(tree_awaiting_input_ack_, tree == tree_
                                           ? mojom::EventResult::HANDLED
                                           : mojom::EventResult::UNHANDLED);
}

WindowManagerState::WindowManagerState(Display* display,
                                       PlatformDisplay* platform_display,
                                       cc::SurfaceId surface_id,
                                       bool is_user_id_valid,
                                       const UserId& user_id)
    : display_(display),
      platform_display_(platform_display),
      is_user_id_valid_(is_user_id_valid),
      user_id_(user_id),
      event_dispatcher_(this) {
  frame_decoration_values_ = mojom::FrameDecorationValues::New();
  frame_decoration_values_->normal_client_area_insets = mojo::Insets::New();
  frame_decoration_values_->maximized_client_area_insets = mojo::Insets::New();
  frame_decoration_values_->max_title_bar_button_width = 0u;

  root_.reset(window_server()->CreateServerWindow(
      window_server()->display_manager()->GetAndAdvanceNextRootId(),
      ServerWindow::Properties()));
  // Our root is always a child of the Display's root. Do this
  // before the WindowTree has been created so that the client doesn't get
  // notified of the add, bounds change and visibility change.
  root_->SetBounds(gfx::Rect(display->root_window()->bounds().size()));
  root_->SetVisible(true);
  display->root_window()->Add(root_.get());

  event_dispatcher_.set_root(root_.get());
  event_dispatcher_.set_surface_id(surface_id);

  AddDebugAccelerators();
}

bool WindowManagerState::IsActive() const {
  return display()->GetActiveWindowManagerState() == this;
}

void WindowManagerState::Activate(const gfx::Point& mouse_location_on_screen) {
  root_->SetVisible(true);
  event_dispatcher_.Reset();
  event_dispatcher_.SetMousePointerScreenLocation(mouse_location_on_screen);
}

void WindowManagerState::Deactivate() {
  root_->SetVisible(false);
  event_dispatcher_.Reset();
  // The tree is no longer active, so no point in dispatching any further
  // events.
  std::queue<std::unique_ptr<QueuedEvent>> event_queue;
  event_queue.swap(event_queue_);
}

void WindowManagerState::ProcessEvent(const ui::Event& event) {
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

void WindowManagerState::OnEventAck(mojom::WindowTree* tree,
                                    mojom::EventResult result) {
  if (tree_awaiting_input_ack_ != tree) {
    // TODO(sad): The ack must have arrived after the timeout. We should do
    // something here, and in OnEventAckTimeout().
    return;
  }
  tree_awaiting_input_ack_ = nullptr;
  event_ack_timer_.Stop();

  if (result == mojom::EventResult::UNHANDLED && post_target_accelerator_)
    OnAccelerator(post_target_accelerator_->id(), *event_awaiting_input_ack_);

  ProcessNextEventFromQueue();
}

WindowServer* WindowManagerState::window_server() {
  return display_->window_server();
}

void WindowManagerState::OnEventAckTimeout() {
  // TODO(sad): Figure out what we should do.
  NOTIMPLEMENTED() << "Event ACK timed out.";
  OnEventAck(tree_awaiting_input_ack_, mojom::EventResult::UNHANDLED);
}

void WindowManagerState::QueueEvent(
    const ui::Event& event,
    std::unique_ptr<ProcessedEventTarget> processed_event_target) {
  std::unique_ptr<QueuedEvent> queued_event(new QueuedEvent);
  queued_event->event = ui::Event::Clone(event);
  queued_event->processed_target = std::move(processed_event_target);
  event_queue_.push(std::move(queued_event));
}

void WindowManagerState::ProcessNextEventFromQueue() {
  // Loop through |event_queue_| stopping after dispatching the first valid
  // event.
  while (!event_queue_.empty()) {
    std::unique_ptr<QueuedEvent> queued_event = std::move(event_queue_.front());
    event_queue_.pop();
    if (!queued_event->processed_target) {
      event_dispatcher_.ProcessEvent(*queued_event->event);
      return;
    }
    if (queued_event->processed_target->IsValid()) {
      DispatchInputEventToWindowImpl(
          queued_event->processed_target->window(),
          queued_event->processed_target->in_nonclient_area(),
          *queued_event->event, queued_event->processed_target->accelerator());
      return;
    }
  }
}

void WindowManagerState::DispatchInputEventToWindowImpl(
    ServerWindow* target,
    bool in_nonclient_area,
    const ui::Event& event,
    base::WeakPtr<Accelerator> accelerator) {
  if (target == root_->parent())
    target = root_.get();

  if (event.IsMousePointerEvent()) {
    DCHECK(event_dispatcher_.mouse_cursor_source_window());
    display_->UpdateNativeCursor(
        event_dispatcher_.mouse_cursor_source_window()->cursor());
  }

  // If the event is in the non-client area the event goes to the owner of
  // the window. Otherwise if the window is an embed root, forward to the
  // embedded window.
  WindowTree* tree =
      in_nonclient_area
          ? window_server()->GetTreeWithId(target->id().connection_id)
          : window_server()->GetTreeWithRoot(target);
  if (!tree) {
    if (in_nonclient_area) {
      // Being the root of the tree means we may get events outside the bounds
      // of the platform window. Because the root has a connection id of 0,
      // no WindowTree is found for it and we have to special case it here.
      DCHECK_EQ(target, root_.get());
      tree = tree_;
    } else {
      tree = window_server()->GetTreeWithId(target->id().connection_id);
    }
  }

  // TOOD(sad): Adjust this delay, possibly make this dynamic.
  const base::TimeDelta max_delay = base::debug::BeingDebugged()
                                        ? base::TimeDelta::FromDays(1)
                                        : GetDefaultAckTimerDelay();
  event_ack_timer_.Start(FROM_HERE, max_delay, this,
                         &WindowManagerState::OnEventAckTimeout);

  tree_awaiting_input_ack_ = tree;
  if (accelerator) {
    event_awaiting_input_ack_ = ui::Event::Clone(event);
    post_target_accelerator_ = accelerator;
  }

  // Ignore |tree| because it will receive the event via normal dispatch.
  window_server()->SendToEventObservers(event, user_id_, tree);

  tree->DispatchInputEvent(target, event);
}

void WindowManagerState::AddDebugAccelerators() {
  // Always register the accelerators, even if they only work in debug, so that
  // keyboard behavior is the same in release and debug builds.
  mojom::EventMatcherPtr matcher = CreateKeyMatcher(
      mus::mojom::KeyboardCode::S,
      mus::mojom::kEventFlagControlDown | mus::mojom::kEventFlagAltDown
          | mus::mojom::kEventFlagShiftDown);
  event_dispatcher_.AddAccelerator(kPrintWindowsDebugAcceleratorId,
                                   std::move(matcher));
}

bool WindowManagerState::HandleDebugAccelerator(uint32_t accelerator_id) {
#if !defined(NDEBUG)
  if (accelerator_id == kPrintWindowsDebugAcceleratorId) {
    // Error so it will be collected in system logs.
    LOG(ERROR) << "ServerWindow hierarchy:\n"
               << root()->GetDebugWindowHierarchy();
    return true;
  }
#endif
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcherDelegate:

void WindowManagerState::OnAccelerator(uint32_t accelerator_id,
                                       const ui::Event& event) {
  DCHECK(IsActive());
  if (HandleDebugAccelerator(accelerator_id))
    return;
  tree_->OnAccelerator(accelerator_id, event);
}

void WindowManagerState::SetFocusedWindowFromEventDispatcher(
    ServerWindow* new_focused_window) {
  DCHECK(IsActive());
  display_->SetFocusedWindow(new_focused_window);
}

ServerWindow* WindowManagerState::GetFocusedWindowForEventDispatcher() {
  return display()->GetFocusedWindow();
}

void WindowManagerState::SetNativeCapture() {
  DCHECK(IsActive());
  platform_display_->SetCapture();
}

void WindowManagerState::ReleaseNativeCapture() {
  platform_display_->ReleaseCapture();
}

void WindowManagerState::OnServerWindowCaptureLost(ServerWindow* window) {
  DCHECK(window);
  window_server()->ProcessLostCapture(window);
}

void WindowManagerState::OnMouseCursorLocationChanged(const gfx::Point& point) {
  window_server()->display_manager()->GetUserDisplayManager(user_id_)->
      OnMouseCursorLocationChanged(point);
}

void WindowManagerState::DispatchInputEventToWindow(ServerWindow* target,
                                                    bool in_nonclient_area,
                                                    const ui::Event& event,
                                                    Accelerator* accelerator) {
  DCHECK(IsActive());
  // TODO(sky): this needs to see if another wms has capture and if so forward
  // to it.
  if (event_ack_timer_.IsRunning()) {
    std::unique_ptr<ProcessedEventTarget> processed_event_target(
        new ProcessedEventTarget(target, in_nonclient_area, accelerator));
    QueueEvent(event, std::move(processed_event_target));
    return;
  }

  base::WeakPtr<Accelerator> weak_accelerator;
  if (accelerator)
    weak_accelerator = accelerator->GetWeakPtr();
  DispatchInputEventToWindowImpl(target, in_nonclient_area, event,
                                 weak_accelerator);
}

void WindowManagerState::OnEventTargetNotFound(const ui::Event& event) {
  window_server()->SendToEventObservers(event, user_id_,
                                        nullptr /* ignore_tree */);
}

}  // namespace ws
}  // namespace mus
