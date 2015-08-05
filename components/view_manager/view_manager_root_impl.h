// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_ROOT_IMPL_H_
#define COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_ROOT_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "components/view_manager/display_manager.h"
#include "components/view_manager/public/cpp/types.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "components/view_manager/server_view.h"

namespace view_manager {

class ConnectionManager;
class ViewManagerRootDelegate;
class ViewManagerServiceImpl;

// ViewManagerRootImpl is an implementation of the ViewManagerRoot interface.
// It serves as a top level root view for a window. Its lifetime is managed by
// ConnectionManager. If the connection to the client breaks or if the user
// closes the associated window, then this object and related state will be
// deleted.
class ViewManagerRootImpl : public DisplayManagerDelegate,
                            public mojo::ViewManagerRoot {
 public:
  // TODO(fsamuel): All these parameters are just plumbing for creating
  // DisplayManagers. We should probably just store these common parameters
  // in the DisplayManagerFactory and pass them along on DisplayManager::Create.
  ViewManagerRootImpl(ConnectionManager* connection_manager,
                      bool is_headless,
                      mojo::ApplicationImpl* app_impl,
                      const scoped_refptr<gles2::GpuState>& gpu_state);
  ~ViewManagerRootImpl() override;

  // Initializes state that depends on the existence of a ViewManagerRootImpl.
  void Init(ViewManagerRootDelegate* delegate);

  ViewManagerServiceImpl* GetViewManagerService();

  mojo::ViewManagerRootClient* client() const { return client_.get(); }

  // Returns whether |view| is a descendant of this root but not itself a
  // root view.
  bool IsViewAttachedToRoot(const ServerView* view) const;

  // Schedules a paint for the specified region in the coordinates of |view| if
  // the |view| is in this viewport. Returns whether |view| is in the viewport.
  bool SchedulePaintIfInViewport(const ServerView* view,
                                 const gfx::Rect& bounds);

  // Returns the metrics for this viewport.
  const mojo::ViewportMetrics& GetViewportMetrics() const;

  ConnectionManager* connection_manager() { return connection_manager_; }

  // Returns the root ServerView of this viewport.
  ServerView* root_view() { return root_.get(); }

  void UpdateTextInputState(const ui::TextInputState& state);
  void SetImeVisibility(bool visible);

  // ViewManagerRoot:
  void SetViewManagerRootClient(mojo::ViewManagerRootClientPtr client) override;
  void SetViewportSize(mojo::SizePtr size) override;
  void CloneAndAnimate(mojo::Id transport_view_id) override;
  void AddAccelerator(mojo::KeyboardCode keyboard_code,
                      mojo::EventFlags flags) override;
  void RemoveAccelerator(mojo::KeyboardCode keyboard_code,
                         mojo::EventFlags flags) override;

 private:
  // DisplayManagerDelegate:
  ServerView* GetRootView() override;
  void OnEvent(mojo::EventPtr event) override;
  void OnDisplayClosed() override;
  void OnViewportMetricsChanged(
      const mojo::ViewportMetrics& old_metrics,
      const mojo::ViewportMetrics& new_metrics) override;

  ViewManagerRootDelegate* delegate_;
  ConnectionManager* const connection_manager_;
  mojo::ViewManagerRootClientPtr client_;
  scoped_ptr<ServerView> root_;
  scoped_ptr<DisplayManager> display_manager_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerRootImpl);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_ROOT_IMPL_H_
