// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_VIEW_TREE_HOST_IMPL_H_
#define COMPONENTS_MUS_WS_VIEW_TREE_HOST_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/event_dispatcher.h"
#include "components/mus/ws/focus_controller_delegate.h"
#include "components/mus/ws/server_view.h"

namespace cc {
class SurfaceManager;
}

namespace mus {
class SurfacesScheduler;
}

namespace mus {

class ConnectionManager;
class FocusController;
class ViewTreeHostDelegate;
class ViewTreeImpl;

// ViewTreeHostImpl is an implementation of the ViewTreeHost interface.
// It serves as a top level root view for a window. Its lifetime is managed by
// ConnectionManager. If the connection to the client breaks or if the user
// closes the associated window, then this object and related state will be
// deleted.
class ViewTreeHostImpl : public DisplayManagerDelegate,
                         public mojom::WindowTreeHost,
                         public FocusControllerDelegate {
 public:
  // TODO(fsamuel): All these parameters are just plumbing for creating
  // DisplayManagers. We should probably just store these common parameters
  // in the DisplayManagerFactory and pass them along on DisplayManager::Create.
  ViewTreeHostImpl(mojom::WindowTreeHostClientPtr client,
                   ConnectionManager* connection_manager,
                   mojo::ApplicationImpl* app_impl,
                   const scoped_refptr<GpuState>& gpu_state,
                   const scoped_refptr<SurfacesState>& surfaces_state);
  ~ViewTreeHostImpl() override;

  // Initializes state that depends on the existence of a ViewTreeHostImpl.
  void Init(ViewTreeHostDelegate* delegate);

  ViewTreeImpl* GetWindowTree();

  mojom::WindowTreeHostClient* client() const { return client_.get(); }

  cc::SurfaceId surface_id() const { return surface_id_; }

  // Returns whether |view| is a descendant of this root but not itself a
  // root view.
  bool IsViewAttachedToRoot(const ServerView* view) const;

  // Schedules a paint for the specified region in the coordinates of |view| if
  // the |view| is in this viewport. Returns whether |view| is in the viewport.
  bool SchedulePaintIfInViewport(const ServerView* view,
                                 const gfx::Rect& bounds);

  // Returns the metrics for this viewport.
  const mojom::ViewportMetrics& GetViewportMetrics() const;

  ConnectionManager* connection_manager() { return connection_manager_; }

  // Returns the root ServerView of this viewport.
  ServerView* root_view() { return root_.get(); }
  const ServerView* root_view() const { return root_.get(); }

  void SetFocusedView(ServerView* view);
  ServerView* GetFocusedView();
  void DestroyFocusController();

  void UpdateTextInputState(ServerView* view, const ui::TextInputState& state);
  void SetImeVisibility(ServerView* view, bool visible);

  void OnAccelerator(uint32_t accelerator_id, mojo::EventPtr event);
  void DispatchInputEventToView(ServerView* target, mojo::EventPtr event);

  // ViewTreeHost:
  void SetSize(mojo::SizePtr size) override;
  void SetTitle(const mojo::String& title) override;
  void AddAccelerator(uint32_t id,
                      mojo::KeyboardCode keyboard_code,
                      mojo::EventFlags flags) override;
  void RemoveAccelerator(uint32_t id) override;

 private:
  void OnClientClosed();

  // DisplayManagerDelegate:
  ServerView* GetRootView() override;
  void OnEvent(mojo::EventPtr event) override;
  void OnDisplayClosed() override;
  void OnViewportMetricsChanged(
      const mojom::ViewportMetrics& old_metrics,
      const mojom::ViewportMetrics& new_metrics) override;
  void OnTopLevelSurfaceChanged(cc::SurfaceId surface_id) override;

  // FocusControllerDelegate:
  void OnFocusChanged(ServerView* old_focused_view,
                      ServerView* new_focused_view) override;

  ViewTreeHostDelegate* delegate_;
  ConnectionManager* const connection_manager_;
  mojom::WindowTreeHostClientPtr client_;
  EventDispatcher event_dispatcher_;
  scoped_ptr<ServerView> root_;
  scoped_ptr<DisplayManager> display_manager_;
  scoped_ptr<FocusController> focus_controller_;
  cc::SurfaceId surface_id_;

  DISALLOW_COPY_AND_ASSIGN(ViewTreeHostImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_VIEW_TREE_HOST_IMPL_H_
