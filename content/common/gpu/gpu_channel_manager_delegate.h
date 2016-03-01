// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_DELEGATE_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_DELEGATE_H_

#include "gpu/command_buffer/common/constants.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace IPC {
struct ChannelHandle;
}

namespace content {

struct AcceleratedSurfaceBuffersSwappedParams;
struct GPUMemoryUmaStats;

class GpuChannelManagerDelegate {
 public:
  // Tells the delegate that a context has subscribed to a new target and
  // the browser should start sending the corresponding information
  virtual void AddSubscription(int32_t client_id, unsigned int target) = 0;

  // Tells the delegate that an offscreen context was created for the provided
  // |active_url|.
  virtual void DidCreateOffscreenContext(const GURL& active_url) = 0;

  // Notification from GPU that the channel is destroyed.
  virtual void DidDestroyChannel(int client_id) = 0;

  // Tells the delegate that an offscreen context was destroyed for the provided
  // |active_url|.
  virtual void DidDestroyOffscreenContext(const GURL& active_url) = 0;

  // Tells the delegate that a context was lost.
  virtual void DidLoseContext(bool offscreen,
                              gpu::error::ContextLostReason reason,
                              const GURL& active_url) = 0;

  // Tells the delegate about GPU memory usage statistics for UMA logging.
  virtual void GpuMemoryUmaStats(const GPUMemoryUmaStats& params) = 0;

  // Tells the delegate that no contexts are subscribed to the target anymore
  // so the delegate should stop sending the corresponding information.
  virtual void RemoveSubscription(int32_t client_id, unsigned int target) = 0;

  // Tells the delegate to cache the given shader information in persistent
  // storage. The embedder is expected to repopulate the in-memory cache through
  // the respective GpuChannelManager API.
  virtual void StoreShaderToDisk(int32_t client_id,
                                 const std::string& key,
                                 const std::string& shader) = 0;

#if defined(OS_MACOSX)
  // Tells the delegate that an accelerated surface has swapped.
  virtual void SendAcceleratedSurfaceBuffersSwapped(
      const AcceleratedSurfaceBuffersSwappedParams& params) = 0;
#endif

#if defined(OS_WIN)
  virtual void SendAcceleratedSurfaceCreatedChildWindow(
      const gfx::PluginWindowHandle& parent_window,
      const gfx::PluginWindowHandle& child_window) = 0;
#endif

 protected:
  virtual ~GpuChannelManagerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_DELEGATE_H_
