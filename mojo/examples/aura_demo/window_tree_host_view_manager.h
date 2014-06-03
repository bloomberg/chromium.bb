// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_H_
#define MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_H_

#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {
namespace examples {

class WindowTreeHostViewManager : public aura::WindowTreeHost,
                                  public ui::EventSource {
 public:
  explicit WindowTreeHostViewManager(const gfx::Rect& bounds);
  virtual ~WindowTreeHostViewManager();

  const gfx::Rect& bounds() const { return bounds_; }

 private:
  // WindowTreeHost:
  virtual ui::EventSource* GetEventSource() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void SetCursorNative(gfx::NativeCursor cursor) OVERRIDE;
  virtual void MoveCursorToNative(const gfx::Point& location) OVERRIDE;
  virtual void OnCursorVisibilityChangedNative(bool show) OVERRIDE;

  // ui::EventSource:
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE;

  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostViewManager);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_AURA_DEMO_WINDOW_TREE_HOST_VIEW_MANAGER_H_
