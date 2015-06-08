// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_DISPLAY_MANAGER_H_
#define COMPONENTS_VIEW_MANAGER_DISPLAY_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/view_manager/native_viewport/platform_viewport.h"
#include "components/view_manager/public/interfaces/display.mojom.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/callback.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class SurfaceIdAllocator;
}  // namespace cc

namespace gles2 {
class GpuState;
}  // namespace gles2

namespace native_viewport {
class OnscreenContextProvider;
}  // namespace native_viewport

namespace mojo {
class ApplicationImpl;
}  // namespace mojo

namespace view_manager {

class EventDispatcher;
class ConnectionManager;
class ServerView;

// DisplayManager is used to connect the root ServerView to a display.
class DisplayManager {
 public:
  virtual ~DisplayManager() {}

  virtual void Init(
      ConnectionManager* connection_manager,
      EventDispatcher* event_dispatcher) = 0;

  // Schedules a paint for the specified region in the coordinates of |view|.
  virtual void SchedulePaint(const ServerView* view,
                             const gfx::Rect& bounds) = 0;

  virtual void SetViewportSize(const gfx::Size& size) = 0;

  virtual const mojo::ViewportMetrics& GetViewportMetrics() = 0;
};

// DisplayManager implementation that connects to the services necessary to
// actually display.
class DefaultDisplayManager :
    public DisplayManager,
    public native_viewport::PlatformViewport::Delegate {
 public:
  DefaultDisplayManager(
      bool is_headless,
      mojo::ApplicationImpl* app_impl,
      const scoped_refptr<gles2::GpuState>& gpu_state,
      const mojo::Callback<void()>& platform_viewport_closed_callback);
  ~DefaultDisplayManager() override;

  // DisplayManager:
  void Init(ConnectionManager* connection_manager,
            EventDispatcher* event_dispatcher) override;
  void SchedulePaint(const ServerView* view, const gfx::Rect& bounds) override;
  void SetViewportSize(const gfx::Size& size) override;
  const mojo::ViewportMetrics& GetViewportMetrics() override;

 private:
  void WantToDraw();
  void Draw();
  void DidDraw();

  // PlatformViewport::Delegate implementation:
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_pixel_ratio) override;
  void OnAcceleratedWidgetDestroyed() override;
  void OnEvent(mojo::EventPtr event) override;
  void OnMetricsChanged(const gfx::Size& size,
                        float device_scale_factor) override;
  void OnDestroyed() override;

  bool is_headless_;
  mojo::ApplicationImpl* app_impl_;
  scoped_refptr<gles2::GpuState> gpu_state_;
  ConnectionManager* connection_manager_;
  EventDispatcher* event_dispatcher_;

  mojo::ViewportMetrics metrics_;
  gfx::Rect dirty_rect_;
  base::Timer draw_timer_;
  bool frame_pending_;

  mojo::DisplayPtr display_;
  scoped_ptr<native_viewport::OnscreenContextProvider> context_provider_;
  scoped_ptr<native_viewport::PlatformViewport> platform_viewport_;
  mojo::Callback<void()> platform_viewport_closed_callback_;

  base::WeakPtrFactory<DefaultDisplayManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultDisplayManager);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_DISPLAY_MANAGER_H_
