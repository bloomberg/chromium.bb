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
#include <set>

#include "base/callback.h"
#include "base/linked_ptr.h"
#include "base/process.h"
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

class Callback0Group
{
 public:
  Callback0Group();
  ~Callback0Group();

  // Add a callback.
  void Add(Callback0::Type* callback);

  // Remove a callback.
  // Returns true if removed, or false if it was not found.
  bool Remove(Callback0::Type* callback);

  // Call all callbacks.
  void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(Callback0Group);

  // Map of callbacks.
  std::set<Callback0::Type*> callbacks_;
};

class GpuProcessHostUIShimManager
{
 public:
  static GpuProcessHostUIShimManager* GetInstance();

  void SetGpuInfo(const GPUInfo& gpu_info);

  // This callback group is invoked when GPU info is updated.
  Callback0Group& gpu_info_update_callbacks() {
    return gpu_info_update_callbacks_;
  }

  // Return all known information about the GPU.
  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

 private:
  GpuProcessHostUIShimManager();
  friend struct DefaultSingletonTraits<GpuProcessHostUIShimManager>;
  DISALLOW_COPY_AND_ASSIGN(GpuProcessHostUIShimManager);

  GPUInfo gpu_info_;
  Callback0Group gpu_info_update_callbacks_;
};

class GpuProcessHostUIShim
    : public IPC::Channel::Listener,
      public IPC::Channel::Sender,
      public base::NonThreadSafe {
 public:
  // Creates a new GpuProcessHostUIShim or gets one for a particular
  // renderer process, resulting in the launching of a GPU process if required.
  // Returns null on failure. It is not safe to store the pointer once control
  // has returned to the message loop as it can be destroyed. Instead store the
  // associated GPU host ID. A renderer ID of zero means the browser process.
  static GpuProcessHostUIShim* GetForRenderer(int renderer_id);

  // Destroy the GpuProcessHostUIShim with the given host ID. This can only
  // be called on the UI thread. Only the GpuProcessHost should destroy the
  // UI shim.
  static void Destroy(int host_id);

  // The GPU process is launched asynchronously. If it launches successfully,
  // this function is called on the UI thread with the process handle. On
  // Windows, the UI shim takes ownership of the handle.
  static void NotifyGpuProcessLaunched(int host_id,
                                       base::ProcessHandle gpu_process);

  static GpuProcessHostUIShim* FromID(int host_id);
  int host_id() const { return host_id_; }

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

  typedef Callback3<const IPC::ChannelHandle&,
                    base::ProcessHandle,
                    const GPUInfo&>::Type
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

#if defined(OS_MACOSX)
  // Notify the GPU process that an accelerated surface was destroyed.
  void DidDestroyAcceleratedSurface(int renderer_id, int32 renderer_route_id);
#endif

  // Sends a message to the browser process to collect the information from the
  // graphics card.
  void CollectGpuInfoAsynchronously(GPUInfo::Level level);

  // Tells the GPU process to crash. Useful for testing.
  void SendAboutGpuCrash();

  // Tells the GPU process to let its main thread enter an infinite loop.
  // Useful for testing.
  void SendAboutGpuHang();

  // Return all known information about the GPU.
  const GPUInfo& gpu_info() const;

  ListValue* log_messages() const { return log_messages_.DeepCopy(); }

  // Can be called directly from the UI thread to log a message.
  void AddCustomLogMessage(int level, const std::string& header,
      const std::string& message);

  bool LoadGpuBlacklist();

 private:
  GpuProcessHostUIShim();
  virtual ~GpuProcessHostUIShim();
  bool Init();

  // Message handlers.
  bool OnControlMessageReceived(const IPC::Message& message);

  void OnChannelEstablished(const IPC::ChannelHandle& channel_handle,
                            const GPUInfo& gpu_info);
  void OnCommandBufferCreated(const int32 route_id);
  void OnDestroyCommandBuffer(gfx::PluginWindowHandle window,
                              int32 renderer_id, int32 render_view_id);
  void OnGraphicsInfoCollected(const GPUInfo& gpu_info);
  void OnPreliminaryGraphicsInfoCollected(
      const GPUInfo& gpu_info, IPC::Message* reply_msg);
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

  // The serial number of the GpuProcessHost / GpuProcessHostUIShim pair.
  int host_id_;

  // The handle for the GPU process or null if it is not known to be launched.
  base::ProcessHandle gpu_process_;

  GPUInfo gpu_info_;
  ListValue log_messages_;

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

  bool gpu_feature_flags_set_;
  scoped_ptr<GpuBlacklist> gpu_blacklist_;
  GpuFeatureFlags gpu_feature_flags_;
};

#endif  // CHROME_BROWSER_GPU_PROCESS_HOST_UI_SHIM_H_

