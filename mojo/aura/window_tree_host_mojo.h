// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_H_
#define MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_H_

#include "base/macros.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/rect.h"

class SkBitmap;

namespace ui {
class Compositor;
}

namespace mojo {

class Shell;
class SurfaceContextFactory;

class WindowTreeHostMojo : public aura::WindowTreeHost,
                           public ui::EventSource,
                           public ViewObserver {
 public:
  WindowTreeHostMojo(Shell* shell, View* view);
  ~WindowTreeHostMojo() override;

  const gfx::Rect& bounds() const { return bounds_; }

  ui::EventDispatchDetails SendEventToProcessor(ui::Event* event) {
    return ui::EventSource::SendEventToProcessor(event);
  }

 private:
  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void Show() override;
  void Hide() override;
  gfx::Rect GetBounds() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Point GetLocationOnNativeScreen() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void PostNativeEvent(const base::NativeEvent& native_event) override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void MoveCursorToNative(const gfx::Point& location) override;
  void OnCursorVisibilityChangedNative(bool show) override;

  // ui::EventSource:
  ui::EventProcessor* GetEventProcessor() override;

  // ViewObserver:
  void OnViewBoundsChanged(View* view,
                           const gfx::Rect& old_bounds,
                           const gfx::Rect& new_bounds) override;

  View* view_;

  gfx::Rect bounds_;

  scoped_ptr<SurfaceContextFactory> context_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMojo);
};

}  // namespace mojo

#endif  // MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_H_
