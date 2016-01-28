// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_TREE_HOST_IMPL_H_
#define COMPONENTS_MUS_WS_WINDOW_TREE_HOST_IMPL_H_

#include <stdint.h>

#include <queue>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/event_dispatcher.h"
#include "components/mus/ws/event_dispatcher_delegate.h"
#include "components/mus/ws/focus_controller_delegate.h"
#include "components/mus/ws/focus_controller_observer.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_observer.h"

namespace mus {
namespace ws {

class ConnectionManager;
class FocusController;
class WindowTreeHostDelegate;
class WindowTreeImpl;

// WindowTreeHostImpl is an implementation of the WindowTreeHost interface.
// It serves as a top level root window for a window. Its lifetime is managed by
// ConnectionManager. If the connection to the client breaks or if the user
// closes the associated window, then this object and related state will be
// deleted.
class WindowTreeHostImpl : public DisplayManagerDelegate,
                           public mojom::WindowTreeHost,
                           public FocusControllerObserver,
                           public FocusControllerDelegate,
                           public EventDispatcherDelegate,
                           public ServerWindowObserver {
 public:
  // TODO(fsamuel): All these parameters are just plumbing for creating
  // DisplayManagers. We should probably just store these common parameters
  // in the DisplayManagerFactory and pass them along on DisplayManager::Create.
  WindowTreeHostImpl(mojom::WindowTreeHostClientPtr client,
                     ConnectionManager* connection_manager,
                     mojo::ApplicationImpl* app_impl,
                     const scoped_refptr<GpuState>& gpu_state,
                     const scoped_refptr<SurfacesState>& surfaces_state,
                     mojom::WindowManagerDeprecatedPtr window_manater);
  ~WindowTreeHostImpl() override;

  // Initializes state that depends on the existence of a WindowTreeHostImpl.
  void Init(WindowTreeHostDelegate* delegate);

  uint32_t id() const { return id_; }

  const WindowTreeImpl* GetWindowTree() const;
  WindowTreeImpl* GetWindowTree();

  mojom::WindowTreeHostClient* client() const { return client_.get(); }

  void SetFrameDecorationValues(mojom::FrameDecorationValuesPtr values);
  const mojom::FrameDecorationValues& frame_decoration_values() const {
    return *frame_decoration_values_;
  }

  // Returns whether |window| is a descendant of this root but not itself a
  // root window.
  bool IsWindowAttachedToRoot(const ServerWindow* window) const;

  // Schedules a paint for the specified region in the coordinates of |window|
  // if
  // the |window| is in this viewport. Returns whether |window| is in the
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

  mojom::WindowManagerDeprecated* window_manager() {
    return window_manager_.get();
  }

  // Returns the root ServerWindow of this viewport.
  ServerWindow* root_window() { return root_.get(); }
  const ServerWindow* root_window() const { return root_.get(); }

  void SetFocusedWindow(ServerWindow* window);
  ServerWindow* GetFocusedWindow();
  void DestroyFocusController();

  void UpdateTextInputState(ServerWindow* window,
                            const ui::TextInputState& state);
  void SetImeVisibility(ServerWindow* window, bool visible);

  // Called just before |tree| is destroyed after its connection encounters an
  // error.
  void OnWindowTreeConnectionError(WindowTreeImpl* tree);

  // Called when a client updates a cursor. This will update the cursor on the
  // native display if the cursor is currently under |window|.
  void OnCursorUpdated(ServerWindow* window);

  // Called when the window tree when stacking and bounds of a window
  // change. This may update the cursor if the ServerWindow under the last
  // known pointer location changed.
  void MaybeChangeCursorOnWindowTreeChange();

  // WindowTreeHost:
  void SetSize(mojo::SizePtr size) override;
  void SetTitle(const mojo::String& title) override;
  void AddAccelerator(uint32_t id,
                      mojom::EventMatcherPtr event_matcher,
                      const AddAcceleratorCallback& callback) override;
  void RemoveAccelerator(uint32_t id) override;
  void AddActivationParent(Id transport_window_id) override;
  void RemoveActivationParent(Id transport_window_id) override;
  void ActivateNextWindow() override;
  void SetUnderlaySurfaceOffsetAndExtendedHitArea(
      Id window_id,
      int32_t x_offset,
      int32_t y_offset,
      mojo::InsetsPtr hit_area) override;

  void OnEventAck(mojom::WindowTree* tree);

 private:
  class ProcessedEventTarget;
  friend class WindowTreeTest;

  // There are two types of events that may be queued, both occur only when
  // waiting for an ack from a client.
  // . We get an event from the DisplayManager. This results in |event| being
  //   set, but |processed_target| is null.
  // . We get an event from the EventDispatcher. In this case both |event| and
  //   |processed_target| are valid.
  // The second case happens if EventDispatcher generates more than one event
  // at a time.
  struct QueuedEvent {
    QueuedEvent();
    ~QueuedEvent();

    mojom::EventPtr event;
    scoped_ptr<ProcessedEventTarget> processed_target;
  };

  // Returns the ServerWindow with the specified transport id. Use this *only*
  // if the call originates WindowTreeImpl associated with GetWindowTree().
  ServerWindow* GetWindowFromWindowTreeHost(Id transport_window_id);

  void OnClientClosed();

  void OnEventAckTimeout();

  // Schedules an event to be processed later.
  void QueueEvent(mojom::EventPtr event,
                  scoped_ptr<ProcessedEventTarget> processed_event_target);

  // Processes the next valid event in |event_queue_|. If the event has already
  // been processed it is dispatched, otherwise the event is passed to the
  // EventDispatcher for processing.
  void ProcessNextEventFromQueue();

  // Dispatches the event to the appropriate client and starts the ack timer.
  void DispatchInputEventToWindowImpl(ServerWindow* target,
                                      bool in_nonclient_area,
                                      mojom::EventPtr event);

  void UpdateNativeCursor(int32_t cursor_id);

  // DisplayManagerDelegate:
  ServerWindow* GetRootWindow() override;
  void OnEvent(const ui::Event& event) override;
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
  void OnAccelerator(uint32_t accelerator_id, mojom::EventPtr event) override;
  void SetFocusedWindowFromEventDispatcher(ServerWindow* window) override;
  ServerWindow* GetFocusedWindowForEventDispatcher() override;
  void DispatchInputEventToWindow(ServerWindow* target,
                                  bool in_nonclient_area,
                                  mojom::EventPtr event) override;

  // ServerWindowObserver:
  void OnWindowDestroyed(ServerWindow* window) override;

  const uint32_t id_;
  WindowTreeHostDelegate* delegate_;
  ConnectionManager* const connection_manager_;
  mojom::WindowTreeHostClientPtr client_;
  EventDispatcher event_dispatcher_;
  scoped_ptr<ServerWindow> root_;
  scoped_ptr<DisplayManager> display_manager_;
  scoped_ptr<FocusController> focus_controller_;
  mojom::WindowManagerDeprecatedPtr window_manager_;
  mojom::WindowTree* tree_awaiting_input_ack_;

  // The last cursor set. Used to track whether we need to change the cursor.
  int32_t last_cursor_;

  std::set<WindowId> activation_parents_;

  // Set of windows with surfaces that need to be destroyed once the frame
  // draws.
  std::set<ServerWindow*> windows_needing_frame_destruction_;

  std::queue<scoped_ptr<QueuedEvent>> event_queue_;
  base::OneShotTimer event_ack_timer_;

  mojom::FrameDecorationValuesPtr frame_decoration_values_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostImpl);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_TREE_HOST_IMPL_H_
