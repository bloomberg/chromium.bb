// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_PROCESS_HOST_H_
#define CHROME_BROWSER_GPU_PROCESS_HOST_H_
#pragma once

#include <queue>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/browser_child_process_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/gpu_info.h"
#include "gfx/native_widget_types.h"

struct GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params;
class GPUInfo;

namespace IPC {
struct ChannelHandle;
class Message;
}

class GpuProcessHost : public BrowserChildProcessHost {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuProcessHost* Get();

  // Tells the GPU process to crash. Useful for testing.
  static void SendAboutGpuCrash();

  // Tells the GPU process to let its main thread enter an infinite loop.
  // Useful for testing.
  static void SendAboutGpuHang();

  // Shutdown routine, which should only be called upon process
  // termination.
  static void Shutdown();

  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);

  // Tells the GPU process to create a new channel for communication with a
  // renderer. Will asynchronously send message to object with given routing id
  // on completion.
  void EstablishGpuChannel(int renderer_id,
                           ResourceMessageFilter* filter);

  // Sends a reply message later when the next GpuHostMsg_SynchronizeReply comes
  // in.
  void Synchronize(IPC::Message* reply,
                   ResourceMessageFilter* filter);

  // Return the stored gpu_info as this class the
  // browser's point of contact with the gpu
  GPUInfo gpu_info() const;
 private:
  // Used to queue pending channel requests.
  struct ChannelRequest {
    explicit ChannelRequest(ResourceMessageFilter* filter)
        : filter(filter) {}
    // Used to send the reply message back to the renderer.
    scoped_refptr<ResourceMessageFilter> filter;
  };

  // Used to queue pending synchronization requests.
  struct SynchronizationRequest {
    SynchronizationRequest(IPC::Message* reply,
                           ResourceMessageFilter* filter)
        : reply(reply),
          filter(filter) {}
    // The delayed reply message which needs to be sent to the
    // renderer.
    IPC::Message* reply;

    // Used to send the reply message back to the renderer.
    scoped_refptr<ResourceMessageFilter> filter;
  };

  GpuProcessHost();
  virtual ~GpuProcessHost();

  bool EnsureInitialized();
  bool Init();

  void OnControlMessageReceived(const IPC::Message& message);

  // Message handlers.
  void OnChannelEstablished(const IPC::ChannelHandle& channel_handle,
                            const GPUInfo& gpu_info);
  void OnSynchronizeReply();
  void OnGraphicsInfoCollected(const GPUInfo& gpu_info);
#if defined(OS_LINUX)
  void OnGetViewXID(gfx::NativeViewId id, unsigned long* xid);
#elif defined(OS_MACOSX)
  void OnAcceleratedSurfaceSetIOSurface(
      const GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params& params);
  void OnAcceleratedSurfaceBuffersSwapped(int32 renderer_id,
                                          int32 render_view_id,
                                          gfx::PluginWindowHandle window);
#endif

  void ReplyToRenderer(const IPC::ChannelHandle& channel,
                       const GPUInfo& gpu_info,
                       ResourceMessageFilter* filter);

  // ResourceDispatcherHost::Receiver implementation:
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data);

  virtual bool CanShutdown();

  bool initialized_;
  bool initialized_successfully_;

  // GPUInfo class used for collecting gpu stats
  GPUInfo gpu_info_;

  // These are the channel requests that we have already sent to
  // the GPU process, but haven't heard back about yet.
  std::queue<ChannelRequest> sent_requests_;

  // The pending synchronization requests we need to reply to.
  std::queue<SynchronizationRequest> queued_synchronization_replies_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessHost);
};

#endif  // CHROME_BROWSER_GPU_PROCESS_HOST_H_
