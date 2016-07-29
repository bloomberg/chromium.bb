// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_OBSERVER_H_
#define CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_OBSERVER_H_

#include <vector>

#include "base/macros.h"
#include "content/public/browser/render_process_host_observer.h"
#include "ui/android/window_android_observer.h"

namespace ui {
class WindowAndroid;
}

namespace content {

class SynchronousCompositorHost;

// SynchronousCompositor class that's tied to the lifetime of a
// RenderProcessHost. Responsible for its own lifetime.
class SynchronousCompositorObserver : public RenderProcessHostObserver,
                                      public ui::WindowAndroidObserver {
 public:
  static SynchronousCompositorObserver* GetOrCreateFor(int process_id);

  // RenderProcessHostObserver overrides.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // WindowAndroidObserver overrides.
  void OnCompositingDidCommit() override;
  void OnRootWindowVisibilityChanged(bool visible) override;
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnVSync(base::TimeTicks frame_time,
               base::TimeDelta vsync_period) override;
  void OnActivityStopped() override;
  void OnActivityStarted() override;

  void SyncStateAfterVSync(ui::WindowAndroid* window_android,
                           SynchronousCompositorHost* compositor_host);

 private:
  explicit SynchronousCompositorObserver(int process_id);
  ~SynchronousCompositorObserver() override;

  RenderProcessHost* const render_process_host_;

  // For synchronizing renderer state after a vsync.
  ui::WindowAndroid* window_android_in_vsync_;
  std::vector<SynchronousCompositorHost*>
      compositor_host_pending_renderer_state_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_OBSERVER_H_
