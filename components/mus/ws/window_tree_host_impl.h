// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_TREE_HOST_IMPL_H_
#define COMPONENTS_MUS_WS_WINDOW_TREE_HOST_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/event_dispatcher.h"
#include "components/mus/ws/event_dispatcher_delegate.h"
#include "components/mus/ws/focus_controller_delegate.h"
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
                     mojom::WindowManagerPtr window_manater);
  ~WindowTreeHostImpl() override;

  // Initializes state that depends on the existence of a WindowTreeHostImpl.
  void Init(WindowTreeHostDelegate* delegate);

  WindowTreeImpl* GetWindowTree();

  mojom::WindowTreeHostClient* client() const { return client_.get(); }

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

  ConnectionManager* connection_manager() { return connection_manager_; }

  mojom::WindowManager* window_manager() { return window_manager_.get(); }

  // Returns the root ServerWindow of this viewport.
  ServerWindow* root_window() { return root_.get(); }
  const ServerWindow* root_window() const { return root_.get(); }

  void SetFocusedWindow(ServerWindow* window);
  ServerWindow* GetFocusedWindow();
  void DestroyFocusController();

  void UpdateTextInputState(ServerWindow* window,
                            const ui::TextInputState& state);
  void SetImeVisibility(ServerWindow* window, bool visible);

  // WindowTreeHost:
  void SetSize(mojo::SizePtr size) override;
  void SetTitle(const mojo::String& title) override;
  void AddAccelerator(uint32_t id,
                      mojom::EventMatcherPtr event_matcher) override;
  void RemoveAccelerator(uint32_t id) override;

 private:
  void OnClientClosed();

  // DisplayManagerDelegate:
  ServerWindow* GetRootWindow() override;
  void OnEvent(mojom::EventPtr event) override;
  void OnDisplayClosed() override;
  void OnViewportMetricsChanged(
      const mojom::ViewportMetrics& old_metrics,
      const mojom::ViewportMetrics& new_metrics) override;
  void OnTopLevelSurfaceChanged(cc::SurfaceId surface_id) override;
  void OnCompositorFrameDrawn() override;

  // FocusControllerDelegate:
  void OnFocusChanged(ServerWindow* old_focused_window,
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

  WindowTreeHostDelegate* delegate_;
  ConnectionManager* const connection_manager_;
  mojom::WindowTreeHostClientPtr client_;
  EventDispatcher event_dispatcher_;
  scoped_ptr<ServerWindow> root_;
  scoped_ptr<DisplayManager> display_manager_;
  scoped_ptr<FocusController> focus_controller_;
  mojom::WindowManagerPtr window_manager_;

  // Set of windows with surfaces that need to be destroyed once the frame
  // draws.
  std::set<ServerWindow*> windows_needing_frame_destruction_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostImpl);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_TREE_HOST_IMPL_H_
