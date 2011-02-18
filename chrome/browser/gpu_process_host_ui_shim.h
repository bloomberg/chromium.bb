// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_PROCESS_HOST_UI_SHIM_H_
#define CHROME_BROWSER_GPU_PROCESS_HOST_UI_SHIM_H_
#pragma once

// This class lives on the UI thread and supports classes like the
// BackingStoreProxy, which must live on the UI thread. The IO thread
// portion of this class, the GpuProcessHost, is responsible for
// shuttling messages between the browser and GPU processes.

#include <map>
#include <queue>

#include "base/callback.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/values.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/common/gpu_feature_flags.h"
#include "chrome/common/gpu_info.h"
#include "chrome/common/message_router.h"
#include "ipc/ipc_channel.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Size;
}

class GpuBlacklist;
struct GPUCreateCommandBufferConfig;
class GPUInfo;
struct GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params;
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;

namespace IPC {
struct ChannelHandle;
class Message;
}

class GpuProcessHostUIShim : public IPC::Channel::Sender,
                             public IPC::Channel::Listener,
                             public base::NonThreadSafe {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuProcessHostUIShim* GetInstance();

  int32 GetNextRoutingId();

  // IPC::Channel::Sender implementation.
  virtual bool Send(IPC::Message* msg);

  // Sends outstanding replies. This is only called
  // in error situations like the GPU process crashing -- but is necessary
  // to prevent the blocked clients from hanging.
  void SendOutstandingReplies();

  // IPC::Channel::Listener implementation.
  // The GpuProcessHost causes this to be called on the UI thread to
  // dispatch the incoming messages from the GPU process, which are
  // actually received on the IO thread.
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

  // See documentation on MessageRouter for AddRoute and RemoveRoute
  void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);
  void RemoveRoute(int32 routing_id);

  // Sends a message to the browser process to collect the information from the
  // graphics card.
  void CollectGraphicsInfoAsynchronously(GPUInfo::Level level);

  // Tells the GPU process to crash. Useful for testing.
  void SendAboutGpuCrash();

  // Tells the GPU process to let its main thread enter an infinite loop.
  // Useful for testing.
  void SendAboutGpuHang();

  // Return all known information about the GPU.
  const GPUInfo& gpu_info() const;

  // Used only in testing. Sets a callback to invoke when GPU info is collected,
  // regardless of whether it has been collected already or if it is partial
  // or complete info. Set to NULL when the callback should no longer be called.
  void set_gpu_info_collected_callback(Callback0::Type* callback) {
    gpu_info_collected_callback_.reset(callback);
  }

  ListValue* logMessages() const { return log_messages_.DeepCopy(); }

  // Can be called directly from the UI thread to log a message.
  void AddCustomLogMessage(int level, const std::string& header,
      const std::string& message);

  bool LoadGpuBlacklist();

 private:
  friend struct DefaultSingletonTraits<GpuProcessHostUIShim>;

  GpuProcessHostUIShim();
  virtual ~GpuProcessHostUIShim();

  // TODO(apatrick): Following the pattern from GpuProcessHost. Talk to zmo
  // and see if we can find a better mechanism.
  bool EnsureInitialized();
  bool Init();

  // Message handlers.
  bool OnControlMessageReceived(const IPC::Message& message);

  void OnChannelEstablished(const IPC::ChannelHandle& channel_handle,
                            const GPUInfo& gpu_info);
  void OnCommandBufferCreated(const int32 route_id);
  void OnDestroyCommandBuffer(gfx::PluginWindowHandle window,
                              int32 renderer_id, int32 render_view_id);
  void OnGraphicsInfoCollected(const GPUInfo& gpu_info);
  void OnLogMessage(int level, const std::string& header,
      const std::string& message);
  void OnSynchronizeReply();
#if defined(OS_LINUX)
  void OnResizeXID(unsigned long xid, gfx::Size size, IPC::Message* reply_msg);
#elif defined(OS_MACOSX)
  void OnAcceleratedSurfaceSetIOSurface(
      const GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params& params);
  void OnAcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params);
#elif defined(OS_WIN)
  void OnScheduleComposite(int32 renderer_id, int32 render_view_id);
#endif

  int last_routing_id_;

  GPUInfo gpu_info_;
  ListValue log_messages_;

  MessageRouter router_;

  // Used only in testing. If set, the callback is invoked when the GPU info
  // has been collected.
  scoped_ptr<Callback0::Type> gpu_info_collected_callback_;

  // These are the channel requests that we have already sent to
  // the GPU process, but haven't heard back about yet.
  std::queue<linked_ptr<EstablishChannelCallback> > channel_requests_;

  // The pending synchronization requests we need to reply to.
  std::queue<linked_ptr<SynchronizeCallback> > synchronize_requests_;

  // The pending create command buffer requests we need to reply to.
  std::queue<linked_ptr<CreateCommandBufferCallback> >
      create_command_buffer_requests_;

  typedef std::pair<int32 /* renderer_id */,
                    int32 /* render_view_id */> ViewID;

  // Encapsulates surfaces that we acquire when creating view command buffers.
  // We assume that a render view has at most 1 such surface associated
  // with it.
  class ViewSurface;
  std::map<ViewID, linked_ptr<ViewSurface> > acquired_surfaces_;

  bool initialized_;
  bool initialized_successfully_;

  bool gpu_feature_flags_set_;
  scoped_ptr<GpuBlacklist> gpu_blacklist_;
  GpuFeatureFlags gpu_feature_flags_;
};

#endif  // CHROME_BROWSER_GPU_PROCESS_HOST_UI_SHIM_H_

