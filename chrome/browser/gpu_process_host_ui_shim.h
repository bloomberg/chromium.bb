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

#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/common/gpu_info.h"
#include "chrome/common/message_router.h"
#include "ipc/ipc_channel.h"
#include "gfx/native_widget_types.h"

namespace gfx {
class Size;
}

struct GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params;
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;

class GpuProcessHostUIShim : public IPC::Channel::Sender,
                             public IPC::Channel::Listener,
                             public base::NonThreadSafe {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuProcessHostUIShim* GetInstance();

  int32 GetNextRoutingId();

  // IPC::Channel::Sender implementation.
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener implementation.
  // The GpuProcessHost causes this to be called on the UI thread to
  // dispatch the incoming messages from the GPU process, which are
  // actually received on the IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message);

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

 private:
  friend struct DefaultSingletonTraits<GpuProcessHostUIShim>;

  GpuProcessHostUIShim();
  virtual ~GpuProcessHostUIShim();

  // Message handlers.
  void OnGraphicsInfoCollected(const GPUInfo& gpu_info);
  bool OnControlMessageReceived(const IPC::Message& message);

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
  void OnScheduleComposite(int32 renderer_id, int32 render_view_id);
#endif

  int last_routing_id_;

  GPUInfo gpu_info_;

  MessageRouter router_;

  // Used only in testing. If set, the callback is invoked when the GPU info
  // has been collected.
  scoped_ptr<Callback0::Type> gpu_info_collected_callback_;
};

#endif  // CHROME_BROWSER_GPU_PROCESS_HOST_UI_SHIM_H_

