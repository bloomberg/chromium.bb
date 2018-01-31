// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_BROWSER_FILTER_H_
#define CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_BROWSER_FILTER_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class RenderProcessHost;
class SynchronousCompositorSyncCallBridge;

class SynchronousCompositorBrowserFilter : public BrowserMessageFilter {
 public:
  explicit SynchronousCompositorBrowserFilter(int process_id);

  // BrowserMessageFilter overrides.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;
  void OnChannelError() override;

  void RegisterSyncCallBridge(
      int routing_id,
      scoped_refptr<SynchronousCompositorSyncCallBridge> host_bridge);
  void UnregisterSyncCallBridge(int routing_id);

 private:
  ~SynchronousCompositorBrowserFilter() override;

  bool ReceiveFrame(const IPC::Message& message);
  bool BeginFrameResponse(const IPC::Message& message);
  void SignalFilterClosed();

  RenderProcessHost* const render_process_host_;

  std::map<int, scoped_refptr<SynchronousCompositorSyncCallBridge>> bridges_;

  bool filter_ready_ = false;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorBrowserFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_BROWSER_FILTER_H_
