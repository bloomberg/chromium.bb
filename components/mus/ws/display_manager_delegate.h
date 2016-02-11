// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_
#define COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_

#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/ws/ids.h"

namespace cc {
struct SurfaceId;
}

namespace ui {
class Event;
}

namespace mus {

namespace ws {

class ServerWindow;

// A DisplayManagerDelegate an interface to be implemented by an object that
// manages the lifetime of a DisplayManager, forwards events to the appropriate
// windows, and responses to changes in viewport size.
class DisplayManagerDelegate {
 public:
  // Returns the root window of this display.
  virtual ServerWindow* GetRootWindow() = 0;

  // Called when the window managed by the DisplayManager is closed.
  virtual void OnDisplayClosed() = 0;

  // Called when an event arrives.
  virtual void OnEvent(const ui::Event& event) = 0;

  // Called when the Display loses capture.
  virtual void OnNativeCaptureLost() = 0;

  // Signals that the metrics of this display's viewport has changed.
  virtual void OnViewportMetricsChanged(
      const mojom::ViewportMetrics& old_metrics,
      const mojom::ViewportMetrics& new_metrics) = 0;

  virtual void OnTopLevelSurfaceChanged(cc::SurfaceId surface_id) = 0;

  // Called when a compositor frame is finished drawing.
  virtual void OnCompositorFrameDrawn() = 0;

 protected:
  virtual ~DisplayManagerDelegate() {}
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_
