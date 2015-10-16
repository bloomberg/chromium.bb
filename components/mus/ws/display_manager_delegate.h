// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_
#define COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_

#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/mus/ws/ids.h"

namespace cc {
struct SurfaceId;
}

namespace mus {

class ServerView;

// A DisplayManagerDelegate an interface to be implemented by an object that
// manages the lifetime of a DisplayManager, forwards events to the appropriate
// views, and responses to changes in viewport size.
class DisplayManagerDelegate {
 public:
  // Returns the root view of this display.
  virtual ServerView* GetRootView() = 0;

  // Called when the window managed by the DisplayManager is closed.
  virtual void OnDisplayClosed() = 0;

  // Called when an event arrives.
  virtual void OnEvent(mojo::EventPtr event) = 0;

  // Signals that the metrics of this display's viewport has changed.
  virtual void OnViewportMetricsChanged(
      const mojo::ViewportMetrics& old_metrics,
      const mojo::ViewportMetrics& new_metrics) = 0;

  virtual void OnTopLevelSurfaceChanged(cc::SurfaceId surface_id) = 0;

 protected:
  virtual ~DisplayManagerDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_
