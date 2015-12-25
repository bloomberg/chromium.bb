// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/browser/browser_message_filter.h"

namespace gpu {
struct GPUInfo;
}

namespace content {

// A message filter for messages from the renderer to the GpuProcessHost
// in the browser. Such messages are typically destined for the GPU process,
// but need to be mediated by the browser.
class GpuMessageFilter : public BrowserMessageFilter {
 public:
  GpuMessageFilter(int render_process_id);

  // BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<GpuMessageFilter>;

  ~GpuMessageFilter() override;

  // Message handlers called on the browser IO thread:
  void OnEstablishGpuChannel(CauseForGpuLaunch,
                             IPC::Message* reply);
  // Helper callbacks for the message handlers.
  void EstablishChannelCallback(scoped_ptr<IPC::Message> reply,
                                const IPC::ChannelHandle& channel,
                                const gpu::GPUInfo& gpu_info);

  int gpu_process_id_;
  int render_process_id_;

  base::WeakPtrFactory<GpuMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
