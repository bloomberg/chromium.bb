// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_MANAGER_H_
#define COMPONENTS_MUS_WS_DISPLAY_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/surfaces/top_level_display_client.h"
#include "components/mus/ws/display_manager_delegate.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/callback.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace cc {
class SurfaceIdAllocator;
class SurfaceManager;
}  // namespace cc

namespace gles2 {
class GpuState;
}  // namespace gles2

namespace mojo {
class ApplicationImpl;
}  // namespace mojo

namespace ui {
class PlatformWindow;
struct TextInputState;
}  // namespace ui

namespace mus {

class DisplayManagerFactory;
class EventDispatcher;
class ServerView;
class SurfacesScheduler;
class SurfacesState;

// DisplayManager is used to connect the root ServerView to a display.
class DisplayManager {
 public:
  virtual ~DisplayManager() {}

  static DisplayManager* Create(
      mojo::ApplicationImpl* app_impl,
      const scoped_refptr<GpuState>& gpu_state,
      const scoped_refptr<SurfacesState>& surfaces_state);

  virtual void Init(DisplayManagerDelegate* delegate) = 0;

  // Schedules a paint for the specified region in the coordinates of |view|.
  virtual void SchedulePaint(const ServerView* view,
                             const gfx::Rect& bounds) = 0;

  virtual void SetViewportSize(const gfx::Size& size) = 0;

  virtual void SetTitle(const base::string16& title) = 0;

  virtual const mojom::ViewportMetrics& GetViewportMetrics() = 0;

  virtual void UpdateTextInputState(const ui::TextInputState& state) = 0;
  virtual void SetImeVisibility(bool visible) = 0;

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(DisplayManagerFactory* factory) {
    DisplayManager::factory_ = factory;
  }

 private:
  // Static factory instance (always NULL for non-test).
  static DisplayManagerFactory* factory_;
};

// DisplayManager implementation that connects to the services necessary to
// actually display.
class DefaultDisplayManager : public DisplayManager,
                              public ui::PlatformWindowDelegate {
 public:
  DefaultDisplayManager(mojo::ApplicationImpl* app_impl,
                        const scoped_refptr<GpuState>& gpu_state,
                        const scoped_refptr<SurfacesState>& surfaces_state);
  ~DefaultDisplayManager() override;

  // DisplayManager:
  void Init(DisplayManagerDelegate* delegate) override;
  void SchedulePaint(const ServerView* view, const gfx::Rect& bounds) override;
  void SetViewportSize(const gfx::Size& size) override;
  void SetTitle(const base::string16& title) override;
  const mojom::ViewportMetrics& GetViewportMetrics() override;
  void UpdateTextInputState(const ui::TextInputState& state) override;
  void SetImeVisibility(bool visible) override;

 private:
  void WantToDraw();

  // This method initiates a top level redraw of the display.
  // TODO(fsamuel): This should use vblank as a signal rather than a timer
  // http://crbug.com/533042
  void Draw();

  // This is called after cc::Display has completed generating a new frame
  // for the display. TODO(fsamuel): Idle time processing should happen here
  // if there is budget for it.
  void DidDraw();
  void UpdateMetrics(const gfx::Size& size, float device_pixel_ratio);
  scoped_ptr<cc::CompositorFrame> GenerateCompositorFrame();

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override;
  void OnDamageRect(const gfx::Rect& damaged_region) override;
  void DispatchEvent(ui::Event* event) override;
  void OnCloseRequest() override;
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  void OnLostCapture() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_pixel_ratio) override;
  void OnActivationChanged(bool active) override;

  mojo::ApplicationImpl* app_impl_;
  scoped_refptr<GpuState> gpu_state_;
  scoped_refptr<SurfacesState> surfaces_state_;
  DisplayManagerDelegate* delegate_;

  mojom::ViewportMetrics metrics_;
  gfx::Rect dirty_rect_;
  base::Timer draw_timer_;
  bool frame_pending_;

  scoped_ptr<TopLevelDisplayClient> top_level_display_client_;
  scoped_ptr<ui::PlatformWindow> platform_window_;

  base::WeakPtrFactory<DefaultDisplayManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultDisplayManager);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_MANAGER_H_
