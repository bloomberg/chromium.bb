// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_AURA_WINDOW_TREE_HOST_MOJO_H_
#define MANDOLINE_UI_AURA_WINDOW_TREE_HOST_MOJO_H_

#include "base/macros.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/rect.h"

class SkBitmap;

namespace ui {
class Compositor;
}

namespace mojo {
class Shell;
}

namespace mandoline {

class InputMethodMandoline;
class SurfaceContextFactory;

class WindowTreeHostMojo : public aura::WindowTreeHost,
                           public mojo::ViewObserver {
 public:
  WindowTreeHostMojo(mojo::Shell* shell, mojo::View* view);
  ~WindowTreeHostMojo() override;

  const gfx::Rect& bounds() const { return bounds_; }

  ui::EventDispatchDetails SendEventToProcessor(ui::Event* event) {
    return ui::EventSource::SendEventToProcessor(event);
  }

 private:
  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBounds() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Point GetLocationOnNativeScreen() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void MoveCursorToNative(const gfx::Point& location) override;
  void OnCursorVisibilityChangedNative(bool show) override;

  // mojo::ViewObserver:
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;

  mojo::View* view_;

  gfx::Rect bounds_;

  scoped_ptr<InputMethodMandoline> input_method_;

  scoped_ptr<SurfaceContextFactory> context_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMojo);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_WINDOW_TREE_HOST_MOJO_H_
