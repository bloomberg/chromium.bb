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
#include "components/view_manager/public/interfaces/display.mojom.h"
#include "components/view_manager/public/interfaces/native_viewport.mojom.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/callback.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class SurfaceIdAllocator;
}

namespace mojo {
class ApplicationImpl;
}

namespace view_manager {

class ConnectionManager;
class ServerView;

// DisplayManager is used to connect the root ServerView to a display.
class DisplayManager {
 public:
  virtual ~DisplayManager() {}

  virtual void Init(
      ConnectionManager* connection_manager,
      mojo::NativeViewportEventDispatcherPtr event_dispatcher) = 0;

  // Schedules a paint for the specified region in the coordinates of |view|.
  virtual void SchedulePaint(const ServerView* view,
                             const gfx::Rect& bounds) = 0;

  virtual void SetViewportSize(const gfx::Size& size) = 0;

  virtual const mojo::ViewportMetrics& GetViewportMetrics() = 0;
};

// DisplayManager implementation that connects to the services necessary to
// actually display.
class DefaultDisplayManager : public DisplayManager,
                              public mojo::ErrorHandler {
 public:
  DefaultDisplayManager(
      mojo::ApplicationImpl* app_impl,
      const mojo::Callback<void()>& native_viewport_closed_callback);
  ~DefaultDisplayManager() override;

  // DisplayManager:
  void Init(ConnectionManager* connection_manager,
            mojo::NativeViewportEventDispatcherPtr event_dispatcher) override;
  void SchedulePaint(const ServerView* view, const gfx::Rect& bounds) override;
  void SetViewportSize(const gfx::Size& size) override;
  const mojo::ViewportMetrics& GetViewportMetrics() override;

 private:
  void WantToDraw();
  void Draw();
  void DidDraw();

  void OnMetricsChanged(mojo::ViewportMetricsPtr metrics);

  // ErrorHandler:
  void OnConnectionError() override;

  mojo::ApplicationImpl* app_impl_;
  ConnectionManager* connection_manager_;

  mojo::ViewportMetrics metrics_;
  gfx::Rect dirty_rect_;
  base::Timer draw_timer_;
  bool frame_pending_;

  mojo::DisplayPtr display_;
  mojo::NativeViewportPtr native_viewport_;
  mojo::Callback<void()> native_viewport_closed_callback_;
  base::WeakPtrFactory<DefaultDisplayManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultDisplayManager);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_DISPLAY_MANAGER_H_
