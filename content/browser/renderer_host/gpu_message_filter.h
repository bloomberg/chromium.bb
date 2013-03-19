// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_

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

namespace content {
class RenderWidgetHelper;
class RenderWidgetHostViewFrameSubscriber;
struct GPUInfo;

// A message filter for messages from the renderer to the GpuProcessHost(UIShim)
// in the browser. Such messages are typically destined for the GPU process,
// but need to be mediated by the browser.
class GpuMessageFilter : public BrowserMessageFilter {
 public:
  GpuMessageFilter(int render_process_id,
                   RenderWidgetHelper* render_widget_helper);

  // BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // Signals that the handle for a surface id was updated, and it may be time to
  // unblock existing CreateViewCommandBuffer requests using that surface.
  void SurfaceUpdated(int32 surface_id);

  // This set of API is used to subscribe to frame presentation events.
  // See RenderWidgetHostViewFrameSubscriber for more details.
  void BeginFrameSubscription(
      int route_id,
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber);
  void EndFrameSubscription(int route_id);

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<GpuMessageFilter>;
  struct CreateViewCommandBufferRequest;
  struct FrameSubscription;

  virtual ~GpuMessageFilter();

  // Message handlers called on the browser IO thread:
  void OnEstablishGpuChannel(CauseForGpuLaunch,
                             IPC::Message* reply);
  void OnCreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      IPC::Message* reply);
  // Helper callbacks for the message handlers.
  void EstablishChannelCallback(IPC::Message* reply,
                                const IPC::ChannelHandle& channel,
                                const GPUInfo& gpu_info);
  void CreateCommandBufferCallback(IPC::Message* reply, int32 route_id);

  void BeginAllFrameSubscriptions();
  void EndAllFrameSubscriptions();
  void BeginFrameSubscriptionInternal(
      linked_ptr<FrameSubscription> subscription);
  void EndFrameSubscriptionInternal(
      linked_ptr<FrameSubscription> subscription);

  int gpu_process_id_;
  int render_process_id_;
  bool share_contexts_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;
  std::vector<linked_ptr<CreateViewCommandBufferRequest> > pending_requests_;

  base::WeakPtrFactory<GpuMessageFilter> weak_ptr_factory_;

  typedef std::vector<linked_ptr<FrameSubscription> > FrameSubscriptionList;
  FrameSubscriptionList frame_subscription_list_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_GPU_MESSAGE_FILTER_H_
