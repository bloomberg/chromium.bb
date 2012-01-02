// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/message_loop_helpers.h"
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

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<GpuMessageFilter>;
  virtual ~GpuMessageFilter();

  // Message handlers called on the browser IO thread:
  void OnEstablishGpuChannel(content::CauseForGpuLaunch,
                             IPC::Message* reply);
  void OnCreateViewCommandBuffer(
      int32 render_view_id,
      const GPUCreateCommandBufferConfig& init_params,
      IPC::Message* reply);
  // Helper callbacks for the message handlers.
  void EstablishChannelCallback(IPC::Message* reply,
                                const IPC::ChannelHandle& channel,
                                base::ProcessHandle gpu_process_for_browser,
                                const content::GPUInfo& gpu_info);
  void CreateCommandBufferCallback(IPC::Message* reply, int32 route_id);

  int gpu_host_id_;
  int render_process_id_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
