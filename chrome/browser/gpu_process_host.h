// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_PROCESS_HOST_H_
#define CHROME_BROWSER_GPU_PROCESS_HOST_H_
#pragma once

#include <queue>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/linked_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/browser_child_process_host.h"
#include "chrome/common/gpu_feature_flags.h"
#include "ui/gfx/native_widget_types.h"

class GpuBlacklist;
struct GPUCreateCommandBufferConfig;
class GPUInfo;

namespace IPC {
struct ChannelHandle;
class Message;
}

class GpuProcessHost : public BrowserChildProcessHost,
                       public base::NonThreadSafe {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuProcessHost* Get();

  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  typedef Callback2<const IPC::ChannelHandle&, const GPUInfo&>::Type
      EstablishChannelCallback;

  // Tells the GPU process to create a new channel for communication with a
  // renderer. Once the GPU process responds asynchronously with the IPC handle
  // and GPUInfo, we call the callback.
  void EstablishGpuChannel(
      int renderer_id, EstablishChannelCallback* callback);

  typedef Callback0::Type SynchronizeCallback;

  // Sends a reply message later when the next GpuHostMsg_SynchronizeReply comes
  // in.
  void Synchronize(SynchronizeCallback* callback);

  typedef Callback1<int32>::Type CreateCommandBufferCallback;

  // Tells the GPU process to create a new command buffer that draws into the
  // window associated with the given renderer.
  void CreateViewCommandBuffer(
      int32 render_view_id,
      int32 renderer_id,
      const GPUCreateCommandBufferConfig& init_params,
      CreateCommandBufferCallback* callback);

  // We need to hop threads when creating the command buffer.
  // Let these tasks access our internals.
  friend class CVCBThreadHopping;

 private:
  GpuProcessHost();
  virtual ~GpuProcessHost();

  bool EnsureInitialized();
  bool Init();

  bool OnControlMessageReceived(const IPC::Message& message);

  // Message handlers.
  void OnChannelEstablished(const IPC::ChannelHandle& channel_handle,
                            const GPUInfo& gpu_info);
  void OnSynchronizeReply();
  void OnCommandBufferCreated(const int32 route_id);

  // Sends outstanding replies to renderer processes. This is only called
  // in error situations like the GPU process crashing -- but is necessary
  // to prevent the renderer process from hanging.
  void SendOutstandingReplies();

  virtual bool CanShutdown();
  virtual void OnChildDied();
  virtual void OnProcessCrashed(int exit_code);

  bool CanLaunchGpuProcess() const;
  bool LaunchGpuProcess();

  bool LoadGpuBlacklist();

  bool initialized_;
  bool initialized_successfully_;

  bool gpu_feature_flags_set_;
  scoped_ptr<GpuBlacklist> gpu_blacklist_;
  GpuFeatureFlags gpu_feature_flags_;

  // These are the channel requests that we have already sent to
  // the GPU process, but haven't heard back about yet.
  std::queue<linked_ptr<EstablishChannelCallback> > channel_requests_;

  // The pending synchronization requests we need to reply to.
  std::queue<linked_ptr<SynchronizeCallback> > synchronize_requests_;

  // The pending create command buffer requests we need to reply to.
  std::queue<linked_ptr<CreateCommandBufferCallback> >
      create_command_buffer_requests_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessHost);
};


#endif  // CHROME_BROWSER_GPU_PROCESS_HOST_H_
