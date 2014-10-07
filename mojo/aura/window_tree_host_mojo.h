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
  virtual ~WindowTreeHostMojo();

  const gfx::Rect& bounds() const { return bounds_; }

  ui::EventDispatchDetails SendEventToProcessor(ui::Event* event) {
    return ui::EventSource::SendEventToProcessor(event);
  }

 private:
  // WindowTreeHost:
  virtual ui::EventSource* GetEventSource() override;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual gfx::Rect GetBounds() const override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual gfx::Point GetLocationOnNativeScreen() const override;
  virtual void SetCapture() override;
  virtual void ReleaseCapture() override;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) override;
  virtual void SetCursorNative(gfx::NativeCursor cursor) override;
  virtual void MoveCursorToNative(const gfx::Point& location) override;
  virtual void OnCursorVisibilityChangedNative(bool show) override;

  // ui::EventSource:
  virtual ui::EventProcessor* GetEventProcessor() override;

  // ViewObserver:
  virtual void OnViewBoundsChanged(View* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) override;

  View* view_;

  gfx::Rect bounds_;

  scoped_ptr<SurfaceContextFactory> context_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMojo);
};

}  // namespace mojo

#endif  // MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_H_
