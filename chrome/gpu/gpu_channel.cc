// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_channel.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_thread.h"
#include "chrome/gpu/gpu_video_service.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

GpuChannel::GpuChannel(GpuThread* gpu_thread, int renderer_id)
    : gpu_thread_(gpu_thread),
      renderer_id_(renderer_id) {
  DCHECK(gpu_thread);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);
}

GpuChannel::~GpuChannel() {
}

void GpuChannel::OnChannelConnected(int32 peer_pid) {
  if (!renderer_process_.Open(peer_pid)) {
    NOTREACHED();
  }
}

bool GpuChannel::OnMessageReceived(const IPC::Message& message) {
  if (log_messages_) {
    VLOG(1) << "received message @" << &message << " on channel @" << this
            << " with type " << message.type();
  }

  if (message.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(message);

  // Fail silently if the GPU process has destroyed while the IPC message was
  // en-route.
  return router_.RouteMessage(message);
}

void GpuChannel::OnChannelError() {
  static_cast<GpuThread*>(ChildThread::current())->RemoveChannel(renderer_id_);
}

bool GpuChannel::Send(IPC::Message* message) {
  if (log_messages_) {
    VLOG(1) << "sending message @" << message << " on channel @" << this
            << " with type " << message->type();
  }

  if (!channel_.get()) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

#if defined(OS_MACOSX)
void GpuChannel::AcceleratedSurfaceBuffersSwapped(
    int32 route_id, uint64 swap_buffers_count) {
  GpuCommandBufferStub* stub = stubs_.Lookup(route_id);
  if (stub == NULL)
    return;
  stub->AcceleratedSurfaceBuffersSwapped(swap_buffers_count);
}

void GpuChannel::DidDestroySurface(int32 renderer_route_id) {
  // Since acclerated views are created in the renderer process and then sent
  // to the browser process during GPU channel construction, it is possible that
  // this is called before a GpuCommandBufferStub for |renderer_route_id| was
  // put into |stubs_|. Hence, do not walk |stubs_| here but instead remember
  // all |renderer_route_id|s this was called for and use them later.
  destroyed_renderer_routes_.insert(renderer_route_id);
}

bool GpuChannel::IsRenderViewGone(int32 renderer_route_id) {
  return destroyed_renderer_routes_.count(renderer_route_id) > 0;
}
#endif

bool GpuChannel::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannel, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateViewCommandBuffer,
        OnCreateViewCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateOffscreenCommandBuffer,
        OnCreateOffscreenCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroyCommandBuffer,
        OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateVideoDecoder,
        OnCreateVideoDecoder)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroyVideoDecoder,
        OnDestroyVideoDecoder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

int GpuChannel::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

void GpuChannel::OnCreateViewCommandBuffer(
    gfx::NativeViewId view_id,
    int32 render_view_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32* route_id) {
  *route_id = MSG_ROUTING_NONE;

#if defined(ENABLE_GPU)

  gfx::PluginWindowHandle handle = gfx::kNullPluginWindow;
#if defined(OS_WIN)
  // TODO(apatrick): We don't actually need the window handle on the Windows
  // platform. At this point, it only indicates to the GpuCommandBufferStub
  // whether onscreen or offscreen rendering is requested. The window handle
  // that will be rendered to is the child compositor window and that window
  // handle is provided by the browser process. Looking at what we are doing on
  // this and other platforms, I think a redesign is in order here. Perhaps
  // on all platforms the renderer just indicates whether it wants onscreen or
  // offscreen rendering and the browser provides whichever platform specific
  // "render target" the GpuCommandBufferStub targets.
  handle = gfx::NativeViewFromId(view_id);
#elif defined(OS_LINUX)
  // Ask the browser for the view's XID.
  gpu_thread_->Send(new GpuHostMsg_GetViewXID(view_id, &handle));
#elif defined(OS_MACOSX)
  // On Mac OS X we currently pass a (fake) PluginWindowHandle for the
  // NativeViewId. We could allocate fake NativeViewIds on the browser
  // side as well, and map between those and PluginWindowHandles, but
  // this seems excessive.
  handle = static_cast<gfx::PluginWindowHandle>(
      static_cast<intptr_t>(view_id));
#else
  // TODO(apatrick): This needs to be something valid for mac and linux.
  // Offscreen rendering will work on these platforms but not rendering to the
  // window.
  DCHECK_EQ(view_id, 0);
#endif

  *route_id = GenerateRouteID();
  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this, handle, NULL, gfx::Size(), init_params.allowed_extensions,
      init_params.attribs, 0, *route_id, renderer_id_, render_view_id));
  router_.AddRoute(*route_id, stub.get());
  stubs_.AddWithID(stub.release(), *route_id);
#endif  // ENABLE_GPU
}

void GpuChannel::OnCreateOffscreenCommandBuffer(
    int32 parent_route_id,
    const gfx::Size& size,
    const GPUCreateCommandBufferConfig& init_params,
    uint32 parent_texture_id,
    int32* route_id) {
#if defined(ENABLE_GPU)
  *route_id = GenerateRouteID();
  GpuCommandBufferStub* parent_stub = NULL;
  if (parent_route_id != 0)
    parent_stub = stubs_.Lookup(parent_route_id);

  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this,
      gfx::kNullPluginWindow,
      parent_stub,
      size,
      init_params.allowed_extensions,
      init_params.attribs,
      parent_texture_id,
      *route_id,
      0, 0));
  router_.AddRoute(*route_id, stub.get());
  stubs_.AddWithID(stub.release(), *route_id);
#else
  *route_id = MSG_ROUTING_NONE;
#endif
}

void GpuChannel::OnDestroyCommandBuffer(int32 route_id) {
#if defined(ENABLE_GPU)
  router_.RemoveRoute(route_id);
  stubs_.Remove(route_id);
#endif
}

void GpuChannel::OnCreateVideoDecoder(int32 context_route_id,
                                      int32 decoder_host_id) {
#if defined(ENABLE_GPU)
  VLOG(1) << "GpuChannel::OnCreateVideoDecoder";
  GpuVideoService* service = GpuVideoService::GetInstance();
  if (service == NULL) {
    // TODO(hclam): Need to send a failure message.
    return;
  }

  // The context ID corresponds to the command buffer used.
  GpuCommandBufferStub* stub = stubs_.Lookup(context_route_id);
  int32 decoder_id = GenerateRouteID();

  // TODO(hclam): Need to be careful about the lifetime of the command buffer
  // decoder.
  bool ret = service->CreateVideoDecoder(
      this, &router_, decoder_host_id, decoder_id,
      stub->processor()->decoder());
  DCHECK(ret) << "Failed to create a GpuVideoDecoder";
#endif
}

void GpuChannel::OnDestroyVideoDecoder(int32 decoder_id) {
#if defined(ENABLE_GPU)
  LOG(ERROR) << "GpuChannel::OnDestroyVideoDecoder";
  GpuVideoService* service = GpuVideoService::GetInstance();
  if (service == NULL)
    return;
  service->DestroyVideoDecoder(&router_, decoder_id);
#endif
}

bool GpuChannel::Init() {
  // Check whether we're already initialized.
  if (channel_.get())
    return true;

  // Map renderer ID to a (single) channel to that process.
  std::string channel_name = GetChannelName();
  channel_.reset(new IPC::SyncChannel(
      channel_name, IPC::Channel::MODE_SERVER, this,
      ChildProcess::current()->io_message_loop(), false,
      ChildProcess::current()->GetShutDownEvent()));

  return true;
}

std::string GpuChannel::GetChannelName() {
  return StringPrintf("%d.r%d.gpu", base::GetCurrentProcId(), renderer_id_);
}

#if defined(OS_POSIX)
int GpuChannel::GetRendererFileDescriptor() {
  int fd = -1;
  if (channel_.get()) {
    fd = channel_->GetClientFileDescriptor();
  }
  return fd;
}
#endif  // defined(OS_POSIX)

