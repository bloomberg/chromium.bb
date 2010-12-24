// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_PROCESS_HOST_H_
#define CHROME_BROWSER_GPU_PROCESS_HOST_H_
#pragma once

#include <queue>

#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "base/ref_counted.h"
#include "chrome/browser/browser_child_process_host.h"
#include "gfx/native_widget_types.h"

struct GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params;
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
class GpuBlacklist;
class GPUInfo;
class RenderMessageFilter;

namespace gfx {
class Size;
}

namespace IPC {
struct ChannelHandle;
class Message;
}

class GpuProcessHost : public BrowserChildProcessHost, public NonThreadSafe {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuProcessHost* Get();

  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Tells the GPU process to create a new channel for communication with a
  // renderer. Will asynchronously send message to object with given routing id
  // on completion.
  void EstablishGpuChannel(int renderer_id, RenderMessageFilter* filter);

  // Sends a reply message later when the next GpuHostMsg_SynchronizeReply comes
  // in.
  void Synchronize(IPC::Message* reply, RenderMessageFilter* filter);

 private:
  // Used to queue pending channel requests.
  struct ChannelRequest {
    explicit ChannelRequest(RenderMessageFilter* filter);
    ~ChannelRequest();

    // Used to send the reply message back to the renderer.
    scoped_refptr<RenderMessageFilter> filter;
  };

  // Used to queue pending synchronization requests.
  struct SynchronizationRequest {
    SynchronizationRequest(IPC::Message* reply, RenderMessageFilter* filter);
    ~SynchronizationRequest();

    // The delayed reply message which needs to be sent to the
    // renderer.
    IPC::Message* reply;

    // Used to send the reply message back to the renderer.
    scoped_refptr<RenderMessageFilter> filter;
  };

  GpuProcessHost();
  virtual ~GpuProcessHost();

  bool EnsureInitialized();
  bool Init();

  bool OnControlMessageReceived(const IPC::Message& message);

  // Message handlers.
  void OnChannelEstablished(const IPC::ChannelHandle& channel_handle,
                            const GPUInfo& gpu_info);
  void OnSynchronizeReply();
#if defined(OS_LINUX)
  void OnGetViewXID(gfx::NativeViewId id, IPC::Message* reply_msg);
  void OnReleaseXID(unsigned long xid);
  void OnResizeXID(unsigned long xid, gfx::Size size, IPC::Message* reply_msg);
#elif defined(OS_MACOSX)
  void OnAcceleratedSurfaceSetIOSurface(
      const GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params& params);
  void OnAcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params);
#elif defined(OS_WIN)
  void OnGetCompositorHostWindow(int renderer_id,
                                    int render_view_id,
                                    IPC::Message* reply_message);
#endif

  // Sends the response for establish channel request to the renderer.
  void SendEstablishChannelReply(const IPC::ChannelHandle& channel,
                                 const GPUInfo& gpu_info,
                                 RenderMessageFilter* filter);
  // Sends the response for synchronization request to the renderer.
  void SendSynchronizationReply(IPC::Message* reply,
                                RenderMessageFilter* filter);

  virtual bool CanShutdown();
  virtual void OnChildDied();
  virtual void OnProcessCrashed(int exit_code);

  bool CanLaunchGpuProcess() const;
  bool LaunchGpuProcess();

  bool LoadGpuBlacklist();

  bool initialized_;
  bool initialized_successfully_;

  scoped_ptr<GpuBlacklist> gpu_blacklist_;

  // These are the channel requests that we have already sent to
  // the GPU process, but haven't heard back about yet.
  std::queue<ChannelRequest> sent_requests_;

  // The pending synchronization requests we need to reply to.
  std::queue<SynchronizationRequest> queued_synchronization_replies_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessHost);
};

#endif  // CHROME_BROWSER_GPU_PROCESS_HOST_H_
