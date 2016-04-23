// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_
#define COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_

#include <stdint.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/mus/public/interfaces/display.mojom.h"
#include "components/mus/ws/event_dispatcher.h"
#include "components/mus/ws/event_dispatcher_delegate.h"
#include "components/mus/ws/user_id.h"
#include "components/mus/ws/window_server.h"

namespace cc {
struct SurfaceId;
}

namespace mus {
namespace ws {

class Accelerator;
class Display;
class PlatformDisplay;
class ServerWindow;
class WindowTree;

namespace test {
class WindowManagerStateTestApi;
}

// Manages the state associated with a connection to a WindowManager for
// a specific user.
class WindowManagerState : public EventDispatcherDelegate {
 public:
  // Creates a WindowManagerState that can host content from any user.
  WindowManagerState(Display* display,
                     PlatformDisplay* platform_display,
                     cc::SurfaceId surface_id);
  WindowManagerState(Display* display,
                     PlatformDisplay* platform_display,
                     cc::SurfaceId surface_id,
                     const UserId& user_id);
  ~WindowManagerState() override;

  bool is_user_id_valid() const { return is_user_id_valid_; }

  const UserId& user_id() const { return user_id_; }

  ServerWindow* root() { return root_.get(); }
  const ServerWindow* root() const { return root_.get(); }

  WindowTree* tree() { return tree_; }
  const WindowTree* tree() const { return tree_; }

  Display* display() { return display_; }
  const Display* display() const { return display_; }

  void SetFrameDecorationValues(mojom::FrameDecorationValuesPtr values);
  const mojom::FrameDecorationValues& frame_decoration_values() const {
    return *frame_decoration_values_;
  }
  bool got_frame_decoration_values() const {
    return got_frame_decoration_values_;
  }

  bool SetCapture(ServerWindow* window, bool in_nonclient_area);
  ServerWindow* capture_window() { return event_dispatcher_.capture_window(); }
  const ServerWindow* capture_window() const {
    return event_dispatcher_.capture_window();
  }

  // Checks if |modal_window| is a visible modal window that blocks current
  // capture window and if that's the case, releases the capture.
  void ReleaseCaptureBlockedByModalWindow(const ServerWindow* modal_window);

  // Checks if the current capture window is blocked by any visible modal window
  // and if that's the case, releases the capture.
  void ReleaseCaptureBlockedByAnyModalWindow();

  // Returns true if this is the WindowManager of the active user.
  bool IsActive() const;

  // TODO(sky): EventDispatcher is really an implementation detail and should
  // not be exposed.
  EventDispatcher* event_dispatcher() { return &event_dispatcher_; }

  void Activate(const gfx::Point& mouse_location_on_screen);
  void Deactivate();

  // Processes an event from PlatformDisplay.
  void ProcessEvent(const ui::Event& event);

  // Called when the ack from an event dispatched to WindowTree |tree| is
  // received.
  // TODO(sky): make this private and use a callback.
  void OnEventAck(mojom::WindowTree* tree, mojom::EventResult result);

  // Returns a mojom::Display for the specified display. WindowManager specific
  // values are not set.
  mojom::DisplayPtr ToMojomDisplay() const;

  void OnWillDestroyTree(WindowTree* tree);

 private:
  class ProcessedEventTarget;
  friend class Display;
  friend class test::WindowManagerStateTestApi;

  // There are two types of events that may be queued, both occur only when
  // waiting for an ack from a client.
  // . We get an event from the PlatformDisplay. This results in |event| being
  //   set, but |processed_target| is null.
  // . We get an event from the EventDispatcher. In this case both |event| and
  //   |processed_target| are valid.
  // The second case happens if EventDispatcher generates more than one event
  // at a time.
  struct QueuedEvent {
    QueuedEvent();
    ~QueuedEvent();

    std::unique_ptr<ui::Event> event;
    std::unique_ptr<ProcessedEventTarget> processed_target;
  };

  WindowManagerState(Display* display,
                     PlatformDisplay* platform_display,
                     cc::SurfaceId surface_id,
                     bool is_user_id_valid,
                     const UserId& user_id);

  WindowServer* window_server();

  void OnEventAckTimeout();

  // Schedules an event to be processed later.
  void QueueEvent(const ui::Event& event,
                  std::unique_ptr<ProcessedEventTarget> processed_event_target);

  // Processes the next valid event in |event_queue_|. If the event has already
  // been processed it is dispatched, otherwise the event is passed to the
  // EventDispatcher for processing.
  void ProcessNextEventFromQueue();

  // Dispatches the event to the appropriate client and starts the ack timer.
  void DispatchInputEventToWindowImpl(ServerWindow* target,
                                      bool in_nonclient_area,
                                      const ui::Event& event,
                                      base::WeakPtr<Accelerator> accelerator);

  // EventDispatcherDelegate:
  void OnAccelerator(uint32_t accelerator_id, const ui::Event& event) override;
  void SetFocusedWindowFromEventDispatcher(ServerWindow* window) override;
  ServerWindow* GetFocusedWindowForEventDispatcher() override;
  void SetNativeCapture() override;
  void ReleaseNativeCapture() override;
  void OnServerWindowCaptureLost(ServerWindow* window) override;
  void DispatchInputEventToWindow(ServerWindow* target,
                                  bool in_nonclient_area,
                                  const ui::Event& event,
                                  Accelerator* accelerator) override;
  void OnEventTargetNotFound(const ui::Event& event) override;

  Display* display_;
  PlatformDisplay* platform_display_;
  // If this was created implicitly by a call
  // WindowTreeHostFactory::CreateWindowTreeHost(), then |is_user_id_valid_|
  // is false.
  const bool is_user_id_valid_;
  const UserId user_id_;
  // Root ServerWindow of this WindowManagerState. |root_| has a parent, the
  // root ServerWindow of the Display.
  std::unique_ptr<ServerWindow> root_;
  WindowTree* tree_ = nullptr;

  // Set to true the first time SetFrameDecorationValues() is received.
  bool got_frame_decoration_values_ = false;
  mojom::FrameDecorationValuesPtr frame_decoration_values_;

  mojom::WindowTree* tree_awaiting_input_ack_ = nullptr;
  std::unique_ptr<ui::Event> event_awaiting_input_ack_;
  base::WeakPtr<Accelerator> post_target_accelerator_;
  std::queue<std::unique_ptr<QueuedEvent>> event_queue_;
  base::OneShotTimer event_ack_timer_;

  EventDispatcher event_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerState);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_
