// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
#pragma once

#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/browser/browser_message_filter.h"
#include "ui/gfx/native_widget_types.h"

class GpuProcessHost;
struct GPUCreateCommandBufferConfig;
class RenderWidgetHelper;

namespace content {
struct GPUInfo;
}  // namespace content

// A message filter for messages from the renderer to the GpuProcessHost(UIShim)
// in the browser. Such messages are typically destined for the GPU process,
// but need to be mediated by the browser.
class GpuMessageFilter : public content::BrowserMessageFilter,
                         public base::SupportsWeakPtr<GpuMessageFilter> {
 public:
  GpuMessageFilter(int render_process_id,
                   RenderWidgetHelper* render_widget_helper);

  // content::BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // Signals that the handle for a surface id was updated, and it may be time to
  // unblock existing CreateViewCommandBuffer requests using that surface.
  void SurfaceUpdated(int32 surface_id);

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<GpuMessageFilter>;
  struct CreateViewCommandBufferRequest;

  virtual ~GpuMessageFilter();

  // Message handlers called on the browser IO thread:
  void OnEstablishGpuChannel(content::CauseForGpuLaunch,
                             IPC::Message* reply);
  void OnCreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      IPC::Message* reply);
  // Helper callbacks for the message handlers.
  void EstablishChannelCallback(IPC::Message* reply,
                                const IPC::ChannelHandle& channel,
                                const content::GPUInfo& gpu_info);
  void CreateCommandBufferCallback(IPC::Message* reply, int32 route_id);

  int gpu_process_id_;
  int render_process_id_;
  bool share_contexts_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;
  std::vector<linked_ptr<CreateViewCommandBufferRequest> > pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
