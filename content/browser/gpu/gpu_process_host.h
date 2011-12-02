// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_
#define CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_
#pragma once

#include <map>
#include <queue>

#include "base/callback_old.h"
#include "base/memory/linked_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/browser/browser_child_process_host.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/common/gpu_info.h"
#include "ui/gfx/native_widget_types.h"

struct GPUCreateCommandBufferConfig;

namespace IPC {
class Message;
}

class GpuMainThread;

class GpuProcessHost : public BrowserChildProcessHost,
                       public base::NonThreadSafe {
 public:
  static bool gpu_enabled() { return gpu_enabled_; }

  // Creates a new GpuProcessHost or gets one for a particular
  // renderer process, resulting in the launching of a GPU process if required.
  // Returns null on failure. It is not safe to store the pointer once control
  // has returned to the message loop as it can be destroyed. Instead store the
  // associated GPU host ID. A renderer ID of zero means the browser process.
  // This could return NULL if GPU access is not allowed (blacklisted).
  static GpuProcessHost* GetForRenderer(int renderer_id,
                                        content::CauseForGpuLaunch cause);

  // Helper function to send the given message to the GPU process on the IO
  // thread.  Calls GetForRenderer and if a host is returned, sends it.
  // Can be called from any thread.
  CONTENT_EXPORT static void SendOnIO(int renderer_id,
                                      content::CauseForGpuLaunch cause,
                                      IPC::Message* message);

  // Get the GPU process host for the GPU process with the given ID. Returns
  // null if the process no longer exists.
  static GpuProcessHost* FromID(int host_id);
  int host_id() const { return host_id_; }

  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // ChildProcessHost implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  typedef Callback3<const IPC::ChannelHandle&,
                    base::ProcessHandle,
                    const content::GPUInfo&>::Type
    EstablishChannelCallback;

  // Tells the GPU process to create a new channel for communication with a
  // renderer. Once the GPU process responds asynchronously with the IPC handle
  // and GPUInfo, we call the callback.
  void EstablishGpuChannel(
      int renderer_id, EstablishChannelCallback* callback);

  typedef Callback1<int32>::Type CreateCommandBufferCallback;

  // Tells the GPU process to create a new command buffer that draws into the
  // window associated with the given renderer.
  void CreateViewCommandBuffer(
      gfx::PluginWindowHandle compositing_surface,
      int32 render_view_id,
      int32 renderer_id,
      const GPUCreateCommandBufferConfig& init_params,
      CreateCommandBufferCallback* callback);

  // Whether this GPU process is set up to use software rendering.
  bool software_rendering();

 private:
  GpuProcessHost(int host_id);
  virtual ~GpuProcessHost();

  bool Init();

  // Post an IPC message to the UI shim's message handler on the UI thread.
  void RouteOnUIThread(const IPC::Message& message);

  virtual void OnProcessLaunched() OVERRIDE;
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;

  // Message handlers.
  void OnChannelEstablished(const IPC::ChannelHandle& channel_handle);
  void OnCommandBufferCreated(const int32 route_id);
  void OnDestroyCommandBuffer(
      gfx::PluginWindowHandle window, int32 renderer_id, int32 render_view_id);
  void OnGraphicsInfoCollected(const content::GPUInfo& gpu_info);

  bool LaunchGpuProcess();

  void SendOutstandingReplies();
  void EstablishChannelError(
      EstablishChannelCallback* callback,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessHandle renderer_process_for_gpu,
      const content::GPUInfo& gpu_info);
  void CreateCommandBufferError(CreateCommandBufferCallback* callback,
                                int32 route_id);

  // The serial number of the GpuProcessHost / GpuProcessHostUIShim pair.
  int host_id_;

  // These are the channel requests that we have already sent to
  // the GPU process, but haven't heard back about yet.
  std::queue<linked_ptr<EstablishChannelCallback> > channel_requests_;

  // The pending create command buffer requests we need to reply to.
  std::queue<linked_ptr<CreateCommandBufferCallback> >
      create_command_buffer_requests_;

#if defined(TOOLKIT_USES_GTK)
  typedef std::pair<int32 /* renderer_id */,
                    int32 /* render_view_id */> ViewID;

  // Encapsulates surfaces that we lock when creating view command buffers.
  // We release this lock once the command buffer (or associated GPU process)
  // is destroyed. This prevents the browser from destroying the surface
  // while the GPU process is drawing to it.

  // Multimap is used to simulate reference counting, see comment in
  // GpuProcessHostUIShim::CreateViewCommandBuffer.
  class SurfaceRef;
  typedef std::multimap<ViewID, linked_ptr<SurfaceRef> > SurfaceRefMap;
  SurfaceRefMap surface_refs_;
#endif

  // Qeueud messages to send when the process launches.
  std::queue<IPC::Message*> queued_messages_;

  // The handle for the GPU process or null if it is not known to be launched.
  base::ProcessHandle gpu_process_;

  // Whether we are running a GPU thread inside the browser process instead
  // of a separate GPU process.
  bool in_process_;

  bool software_rendering_;

  scoped_ptr<GpuMainThread> in_process_gpu_thread_;

  // Master switch for enabling/disabling GPU acceleration for the current
  // browser session. It does not change the acceleration settings for
  // existing tabs, just the future ones.
  CONTENT_EXPORT static bool gpu_enabled_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessHost);
};

#endif  // CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_
