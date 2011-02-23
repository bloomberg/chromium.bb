// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
#pragma once

#include "chrome/browser/browser_message_filter.h"

struct GPUCreateCommandBufferConfig;
class GPUInfo;
class GpuProcessHost;
class GpuProcessHostUIShim;

namespace IPC {
struct ChannelHandle;
}

// A message filter for messages from the renderer to the GpuProcessHost(UIShim)
// in the browser. Such messages are typically destined for the GPU process,
// but need to be mediated by the browser.
class GpuMessageFilter : public BrowserMessageFilter,
                         public base::SupportsWeakPtr<GpuMessageFilter> {
 public:
  explicit GpuMessageFilter(int render_process_id);

  // BrowserMessageFilter methods:
  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread);
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);
  virtual void OnDestruct() const;

 private:
  friend class BrowserThread;
  friend class DeleteTask<GpuMessageFilter>;
  virtual ~GpuMessageFilter();

  // Message handlers called on the browser IO thread:
  void OnEstablishGpuChannel();
  void OnSynchronizeGpu(IPC::Message* reply);
  void OnCreateViewCommandBuffer(
      int32 render_view_id,
      const GPUCreateCommandBufferConfig& init_params,
      IPC::Message* reply);

  int gpu_host_id_;
  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
