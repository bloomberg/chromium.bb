// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_MANAGER_H_
#define COMPONENTS_MUS_WS_DISPLAY_MANAGER_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/ws/display_manager_delegate.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace cc {
class CompositorFrame;
class CopyOutputRequest;
class SurfaceIdAllocator;
class SurfaceManager;
}  // namespace cc

namespace gles2 {
class GpuState;
}  // namespace gles2

namespace mojo {
class Shell;
}  // namespace mojo

namespace ui {
class CursorLoader;
class PlatformWindow;
struct TextInputState;
}  // namespace ui

namespace mus {

class GpuState;
class SurfacesState;
class TopLevelDisplayClient;

namespace ws {

class DisplayManagerFactory;
class EventDispatcher;
class ServerWindow;

// DisplayManager is used to connect the root ServerWindow to a display.
// TODO(sky): rename this given we have a mojom type with the same name now.
class DisplayManager {
 public:
  virtual ~DisplayManager() {}

  static DisplayManager* Create(
      mojo::Shell* shell,
      const scoped_refptr<GpuState>& gpu_state,
      const scoped_refptr<SurfacesState>& surfaces_state);

  virtual void Init(DisplayManagerDelegate* delegate) = 0;

  // Schedules a paint for the specified region in the coordinates of |window|.
  virtual void SchedulePaint(const ServerWindow* window,
                             const gfx::Rect& bounds) = 0;

  virtual void SetViewportSize(const gfx::Size& size) = 0;

  virtual void SetTitle(const base::string16& title) = 0;

  virtual void SetCursorById(int32_t cursor) = 0;

  virtual mojom::Rotation GetRotation() = 0;

  virtual const mojom::ViewportMetrics& GetViewportMetrics() = 0;

  virtual void UpdateTextInputState(const ui::TextInputState& state) = 0;
  virtual void SetImeVisibility(bool visible) = 0;

  // Returns true if a compositor frame has been submitted but not drawn yet.
  virtual bool IsFramePending() const = 0;

  virtual void RequestCopyOfOutput(
      scoped_ptr<cc::CopyOutputRequest> output_request) = 0;

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
  DefaultDisplayManager(mojo::Shell* shell,
                        const scoped_refptr<GpuState>& gpu_state,
                        const scoped_refptr<SurfacesState>& surfaces_state);
  ~DefaultDisplayManager() override;

  // DisplayManager:
  void Init(DisplayManagerDelegate* delegate) override;
  void SchedulePaint(const ServerWindow* window,
                     const gfx::Rect& bounds) override;
  void SetViewportSize(const gfx::Size& size) override;
  void SetTitle(const base::string16& title) override;
  void SetCursorById(int32_t cursor) override;
  const mojom::ViewportMetrics& GetViewportMetrics() override;
  mojom::Rotation GetRotation() override;
  void UpdateTextInputState(const ui::TextInputState& state) override;
  void SetImeVisibility(bool visible) override;
  bool IsFramePending() const override;
  void RequestCopyOfOutput(
      scoped_ptr<cc::CopyOutputRequest> output_request) override;

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
  void OnAcceleratedWidgetDestroyed() override;
  void OnActivationChanged(bool active) override;

  mojo::Shell* shell_;
  scoped_refptr<GpuState> gpu_state_;
  scoped_refptr<SurfacesState> surfaces_state_;
  DisplayManagerDelegate* delegate_;

  mojom::ViewportMetrics metrics_;
  gfx::Rect dirty_rect_;
  base::Timer draw_timer_;
  bool frame_pending_;

  scoped_ptr<TopLevelDisplayClient> top_level_display_client_;
  scoped_ptr<ui::PlatformWindow> platform_window_;

#if !defined(OS_ANDROID)
  scoped_ptr<ui::CursorLoader> cursor_loader_;
#endif

  base::WeakPtrFactory<DefaultDisplayManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultDisplayManager);
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_MANAGER_H_
