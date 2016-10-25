// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_BROWSER_FILTER_H_
#define CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_BROWSER_FILTER_H_

#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_message_filter.h"
#include "ui/android/window_android_observer.h"

namespace ui {
class WindowAndroid;
}

namespace content {

class RenderProcessHost;
class SynchronousCompositorHost;

class SynchronousCompositorBrowserFilter : public ui::WindowAndroidObserver,
                                           public BrowserMessageFilter {
 public:
  explicit SynchronousCompositorBrowserFilter(int process_id);

  // WindowAndroidObserver overrides.
  void OnCompositingDidCommit() override;
  void OnRootWindowVisibilityChanged(bool visible) override;
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnVSync(base::TimeTicks frame_time,
               base::TimeDelta vsync_period) override;
  void OnActivityStopped() override;
  void OnActivityStarted() override;

  // BrowserMessageFilter overrides.
  bool OnMessageReceived(const IPC::Message& message) override;

  void SyncStateAfterVSync(ui::WindowAndroid* window_android,
                           SynchronousCompositorHost* compositor_host);
  void SetFrameFuture(
      int routing_id,
      scoped_refptr<SynchronousCompositor::FrameFuture> frame_future);

 private:
  ~SynchronousCompositorBrowserFilter() override;

  bool ReceiveFrame(const IPC::Message& message);

  RenderProcessHost* const render_process_host_;

  // For synchronizing renderer state after a vsync.
  ui::WindowAndroid* window_android_in_vsync_;
  std::vector<SynchronousCompositorHost*>
      compositor_host_pending_renderer_state_;

  base::Lock future_map_lock_;  // Protects |future_map_|.
  using FrameFutureMap =
      std::map<int, scoped_refptr<SynchronousCompositor::FrameFuture>>;
  FrameFutureMap future_map_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorBrowserFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_BROWSER_FILTER_H_
