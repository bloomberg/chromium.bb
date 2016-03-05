// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_H_
#define COMPONENTS_MUS_WS_DISPLAY_H_

#include <stdint.h>

#include <map>
#include <queue>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/ws/event_dispatcher.h"
#include "components/mus/ws/event_dispatcher_delegate.h"
#include "components/mus/ws/focus_controller_delegate.h"
#include "components/mus/ws/focus_controller_observer.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_observer.h"
#include "components/mus/ws/server_window_tracker.h"
#include "components/mus/ws/user_id_tracker_observer.h"
#include "components/mus/ws/window_manager_factory_registry_observer.h"

namespace mus {
namespace ws {

class ConnectionManager;
class DisplayBinding;
class FocusController;
class WindowManagerState;
class WindowTree;

namespace test {
class DisplayTestApi;
}

// Displays manages the state associated with a single display. Display has a
// single root window whose children are the roots for a per-user
// WindowManager. Display is configured in two distinct
// ways:
// . with a DisplayBinding. In this mode there is only ever one WindowManager
//   for the display, which comes from the client that created the
//   Display.
// . without a DisplayBinding. In this mode a WindowManager is automatically
//   created per user.
class Display : public PlatformDisplayDelegate,
                public mojom::WindowTreeHost,
                public FocusControllerObserver,
                public FocusControllerDelegate,
                public EventDispatcherDelegate,
                public ServerWindowObserver,
                public UserIdTrackerObserver,
                public WindowManagerFactoryRegistryObserver {
 public:
  // TODO(fsamuel): All these parameters are just plumbing for creating
  // PlatformDisplays. We should probably just store these common parameters
  // in the PlatformDisplayFactory and pass them along on
  // PlatformDisplay::Create.
  Display(ConnectionManager* connection_manager,
          mojo::Connector* connector,
          const scoped_refptr<GpuState>& gpu_state,
          const scoped_refptr<SurfacesState>& surfaces_state);
  ~Display() override;

  // Initializes state that depends on the existence of a Display.
  void Init(scoped_ptr<DisplayBinding> binding);

  uint32_t id() const { return id_; }

  // TODO(sky): move to WMM.
  void SetFrameDecorationValues(mojom::FrameDecorationValuesPtr values);
  const mojom::FrameDecorationValues& frame_decoration_values() const {
    return *frame_decoration_values_;
  }

  // Schedules a paint for the specified region in the coordinates of |window|
  // if the |window| is in this viewport. Returns whether |window| is in the
  // viewport.
  bool SchedulePaintIfInViewport(const ServerWindow* window,
                                 const gfx::Rect& bounds);

  // Schedules destruction of surfaces in |window|. If a frame has been
  // scheduled but not drawn surface destruction is delayed until the frame is
  // drawn, otherwise destruction is immediate.
  void ScheduleSurfaceDestruction(ServerWindow* window);

  // Returns the metrics for this viewport.
  const mojom::ViewportMetrics& GetViewportMetrics() const;

  mojom::Rotation GetRotation() const;

  ConnectionManager* connection_manager() { return connection_manager_; }

  EventDispatcher* event_dispatcher() { return &event_dispatcher_; }

  // Returns the root of the Display. The root's children are the roots
  // of the corresponding WindowManagers.
  ServerWindow* root_window() { return root_.get(); }
  const ServerWindow* root_window() const { return root_.get(); }

  ServerWindow* GetRootWithId(const WindowId& id);

  WindowManagerState* GetWindowManagerStateWithRoot(const ServerWindow* window);
  // TODO(sky): this is wrong, plumb through user_id.
  WindowManagerState* GetFirstWindowManagerState();
  WindowManagerState* GetWindowManagerStateForUser(const UserId& user_id);
  size_t num_window_manger_states() const {
    return window_manager_state_map_.size();
  }

  void SetCapture(ServerWindow* window, bool in_nonclient_area);

  void SetFocusedWindow(ServerWindow* window);
  ServerWindow* GetFocusedWindow();
  void DestroyFocusController();
  FocusController* focus_controller() { return focus_controller_.get(); }

  void AddActivationParent(ServerWindow* window);
  void RemoveActivationParent(ServerWindow* window);

  void UpdateTextInputState(ServerWindow* window,
                            const ui::TextInputState& state);
  void SetImeVisibility(ServerWindow* window, bool visible);

  // Returns the window that has captured input.
  ServerWindow* GetCaptureWindow() {
    return event_dispatcher_.capture_window();
  }
  const ServerWindow* GetCaptureWindow() const {
    return event_dispatcher_.capture_window();
  }

  // Called just before |tree| is destroyed after its connection encounters an
  // error.
  void OnWindowTreeConnectionError(WindowTree* tree);

  // Called when a client updates a cursor. This will update the cursor on the
  // native display if the cursor is currently under |window|.
  void OnCursorUpdated(ServerWindow* window);

  // Called when the window tree when stacking and bounds of a window
  // change. This may update the cursor if the ServerWindow under the last
  // known pointer location changed.
  void MaybeChangeCursorOnWindowTreeChange();

  // mojom::WindowTreeHost:
  void SetSize(mojo::SizePtr size) override;
  void SetTitle(const mojo::String& title) override;

  void OnEventAck(mojom::WindowTree* tree);

 private:
  class ProcessedEventTarget;
  friend class test::DisplayTestApi;

  using WindowManagerStateMap =
      std::map<UserId, scoped_ptr<WindowManagerState>>;

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

    scoped_ptr<ui::Event> event;
    scoped_ptr<ProcessedEventTarget> processed_target;
  };

  // Inits the necessary state once the display is ready.
  void InitWindowManagersIfNecessary();

  void OnEventAckTimeout();

  // Schedules an event to be processed later.
  void QueueEvent(const ui::Event& event,
                  scoped_ptr<ProcessedEventTarget> processed_event_target);

  // Processes the next valid event in |event_queue_|. If the event has already
  // been processed it is dispatched, otherwise the event is passed to the
  // EventDispatcher for processing.
  void ProcessNextEventFromQueue();

  // Dispatches the event to the appropriate client and starts the ack timer.
  void DispatchInputEventToWindowImpl(ServerWindow* target,
                                      bool in_nonclient_area,
                                      const ui::Event& event);

  // Creates the set of WindowManagerStates from the
  // WindowManagerFactoryRegistry.
  void CreateWindowManagerStatesFromRegistry();

  void CreateWindowManagerStateFromService(
      WindowManagerFactoryService* service);

  void UpdateNativeCursor(int32_t cursor_id);

  // PlatformDisplayDelegate:
  ServerWindow* GetRootWindow() override;
  void OnEvent(const ui::Event& event) override;
  void OnNativeCaptureLost() override;
  void OnDisplayClosed() override;
  void OnViewportMetricsChanged(
      const mojom::ViewportMetrics& old_metrics,
      const mojom::ViewportMetrics& new_metrics) override;
  void OnTopLevelSurfaceChanged(cc::SurfaceId surface_id) override;
  void OnCompositorFrameDrawn() override;

  // FocusControllerDelegate:
  bool CanHaveActiveChildren(ServerWindow* window) const override;

  // FocusControllerObserver:
  void OnActivationChanged(ServerWindow* old_active_window,
                           ServerWindow* new_active_window) override;
  void OnFocusChanged(FocusControllerChangeSource change_source,
                      ServerWindow* old_focused_window,
                      ServerWindow* new_focused_window) override;

  // EventDispatcherDelegate:
  void OnAccelerator(uint32_t accelerator_id, const ui::Event& event) override;
  void SetFocusedWindowFromEventDispatcher(ServerWindow* window) override;
  ServerWindow* GetFocusedWindowForEventDispatcher() override;
  void SetNativeCapture() override;
  void ReleaseNativeCapture() override;
  void OnServerWindowCaptureLost(ServerWindow* window) override;
  void DispatchInputEventToWindow(ServerWindow* target,
                                  bool in_nonclient_area,
                                  const ui::Event& event) override;

  // ServerWindowObserver:
  void OnWindowDestroyed(ServerWindow* window) override;

  // UserIdTrackerObserver:
  void OnActiveUserIdChanged(const UserId& id) override;
  void OnUserIdAdded(const UserId& id) override;
  void OnUserIdRemoved(const UserId& id) override;

  // WindowManagerFactoryRegistryObserver:
  void OnWindowManagerFactorySet(WindowManagerFactoryService* service) override;

  const uint32_t id_;
  scoped_ptr<DisplayBinding> binding_;
  // Set once Init() has been called.
  bool init_called_ = false;
  ConnectionManager* const connection_manager_;
  EventDispatcher event_dispatcher_;
  scoped_ptr<ServerWindow> root_;
  scoped_ptr<PlatformDisplay> platform_display_;
  scoped_ptr<FocusController> focus_controller_;
  mojom::WindowTree* tree_awaiting_input_ack_;

  // The last cursor set. Used to track whether we need to change the cursor.
  int32_t last_cursor_;

  ServerWindowTracker activation_parents_;

  // Set of windows with surfaces that need to be destroyed once the frame
  // draws.
  std::set<ServerWindow*> windows_needing_frame_destruction_;

  std::queue<scoped_ptr<QueuedEvent>> event_queue_;
  base::OneShotTimer event_ack_timer_;

  WindowManagerStateMap window_manager_state_map_;

  mojom::FrameDecorationValuesPtr frame_decoration_values_;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_H_
