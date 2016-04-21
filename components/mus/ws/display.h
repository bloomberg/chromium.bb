// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_H_
#define COMPONENTS_MUS_WS_DISPLAY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <queue>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
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

class DisplayBinding;
class DisplayManager;
class FocusController;
struct PlatformDisplayInitParams;
class WindowManagerState;
class WindowServer;
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
                public ServerWindowObserver,
                public UserIdTrackerObserver,
                public WindowManagerFactoryRegistryObserver {
 public:
  Display(WindowServer* window_server,
          const PlatformDisplayInitParams& platform_display_init_params);
  ~Display() override;

  // Initializes state that depends on the existence of a Display.
  void Init(std::unique_ptr<DisplayBinding> binding);

  uint32_t id() const { return id_; }

  DisplayManager* display_manager();
  const DisplayManager* display_manager() const;

  // Returns a mojom::Display for the specified display. WindowManager specific
  // values are not set. In general you should use
  // WindowManagerState::ToMojomDisplay().
  mojom::DisplayPtr ToMojomDisplay() const;

  // Schedules a paint for the specified region in the coordinates of |window|.
  void SchedulePaint(const ServerWindow* window, const gfx::Rect& bounds);

  // Schedules destruction of surfaces in |window|. If a frame has been
  // scheduled but not drawn surface destruction is delayed until the frame is
  // drawn, otherwise destruction is immediate.
  void ScheduleSurfaceDestruction(ServerWindow* window);

  // Returns the metrics for this viewport.
  const mojom::ViewportMetrics& GetViewportMetrics() const;

  mojom::Rotation GetRotation() const;

  WindowServer* window_server() { return window_server_; }

  // Returns the root of the Display. The root's children are the roots
  // of the corresponding WindowManagers.
  ServerWindow* root_window() { return root_.get(); }
  const ServerWindow* root_window() const { return root_.get(); }

  ServerWindow* GetRootWithId(const WindowId& id);

  WindowManagerState* GetWindowManagerStateWithRoot(const ServerWindow* window);
  WindowManagerState* GetWindowManagerStateForUser(const UserId& user_id) {
    return const_cast<WindowManagerState*>(
        const_cast<const Display*>(this)->GetWindowManagerStateForUser(
            user_id));
  }
  const WindowManagerState* GetWindowManagerStateForUser(
      const UserId& user_id) const;
  WindowManagerState* GetActiveWindowManagerState() {
    return const_cast<WindowManagerState*>(
        const_cast<const Display*>(this)->GetActiveWindowManagerState());
  }
  const WindowManagerState* GetActiveWindowManagerState() const;
  size_t num_window_manger_states() const {
    return window_manager_state_map_.size();
  }

  // TODO(sky): this should only be called by WindowServer, move to interface
  // used by WindowServer.
  // See description of WindowServer::SetFocusedWindow() for details on return
  // value.
  bool SetFocusedWindow(ServerWindow* window);
  // NOTE: this returns the focused window only if the focused window is in this
  // display. If this returns null focus may be in another display.
  ServerWindow* GetFocusedWindow();

  void ActivateNextWindow();

  void AddActivationParent(ServerWindow* window);
  void RemoveActivationParent(ServerWindow* window);

  void UpdateTextInputState(ServerWindow* window,
                            const ui::TextInputState& state);
  void SetImeVisibility(ServerWindow* window, bool visible);

  // Called just before |tree| is destroyed.
  void OnWillDestroyTree(WindowTree* tree);

  void UpdateNativeCursor(int32_t cursor_id);

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

 private:
  friend class test::DisplayTestApi;

  using WindowManagerStateMap =
      std::map<UserId, std::unique_ptr<WindowManagerState>>;

  // Inits the necessary state once the display is ready.
  void InitWindowManagersIfNecessary();

  // Creates the set of WindowManagerStates from the
  // WindowManagerFactoryRegistry.
  void CreateWindowManagerStatesFromRegistry();

  void CreateWindowManagerStateFromService(
      WindowManagerFactoryService* service);

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

  // ServerWindowObserver:
  void OnWindowDestroyed(ServerWindow* window) override;

  // UserIdTrackerObserver:
  void OnActiveUserIdChanged(const UserId& previously_active_id,
                             const UserId& active_id) override;
  void OnUserIdAdded(const UserId& id) override;
  void OnUserIdRemoved(const UserId& id) override;

  // WindowManagerFactoryRegistryObserver:
  void OnWindowManagerFactorySet(WindowManagerFactoryService* service) override;

  const uint32_t id_;
  std::unique_ptr<DisplayBinding> binding_;
  // Set once Init() has been called.
  bool init_called_ = false;
  WindowServer* const window_server_;
  std::unique_ptr<ServerWindow> root_;
  std::unique_ptr<PlatformDisplay> platform_display_;
  std::unique_ptr<FocusController> focus_controller_;

  // The last cursor set. Used to track whether we need to change the cursor.
  int32_t last_cursor_;

  ServerWindowTracker activation_parents_;

  // Set of windows with surfaces that need to be destroyed once the frame
  // draws.
  std::set<ServerWindow*> windows_needing_frame_destruction_;

  WindowManagerStateMap window_manager_state_map_;

  cc::SurfaceId top_level_surface_id_;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_H_
