// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "chrome/gpu/gpu_channel.h"

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/gpu/gpu_thread.h"
#include "chrome/gpu/gpu_video_service.h"
#include "content/common/child_process.h"
#include "content/common/gpu_messages.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

GpuChannel::GpuChannel(GpuThread* gpu_thread,
                       int renderer_id)
    : gpu_thread_(gpu_thread),
      renderer_id_(renderer_id),
      renderer_process_(NULL),
      renderer_pid_(NULL) {
  DCHECK(gpu_thread);
  DCHECK(renderer_id);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);
}

GpuChannel::~GpuChannel() {
#if defined(OS_WIN)
  if (renderer_process_)
    CloseHandle(renderer_process_);
#endif
}

bool GpuChannel::OnMessageReceived(const IPC::Message& message) {
  if (log_messages_) {
    VLOG(1) << "received message @" << &message << " on channel @" << this
            << " with type " << message.type();
  }

  if (message.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(message);

  if (!router_.RouteMessage(message)) {
    // Respond to sync messages even if router failed to route.
    if (message.is_sync()) {
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
      reply->set_reply_error();
      Send(reply);
    }
    return false;
  }

  return true;
}

void GpuChannel::OnChannelError() {
  gpu_thread_->RemoveChannel(renderer_id_);
}

void GpuChannel::OnChannelConnected(int32 peer_pid) {
  renderer_pid_ = peer_pid;
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

void GpuChannel::CreateViewCommandBuffer(
    gfx::PluginWindowHandle window,
    int32 render_view_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32* route_id) {
  *route_id = MSG_ROUTING_NONE;

#if defined(ENABLE_GPU)
  *route_id = GenerateRouteID();
  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this, window, NULL, gfx::Size(), init_params.allowed_extensions,
      init_params.attribs, 0, *route_id, renderer_id_, render_view_id));
  router_.AddRoute(*route_id, stub.get());
  stubs_.AddWithID(stub.release(), *route_id);
#endif  // ENABLE_GPU
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
    IPC_MESSAGE_HANDLER(GpuChannelMsg_Initialize, OnInitialize)
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

void GpuChannel::OnInitialize(base::ProcessHandle renderer_process) {
  // Initialize should only happen once.
  DCHECK(!renderer_process_);

  // Verify that the renderer has passed its own process handle.
  if (base::GetProcId(renderer_process) == renderer_pid_)
    renderer_process_ = renderer_process;
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
// TODO(cevans): do NOT re-enable this until GpuVideoService has been checked
// for integer overflows, including the classic "width * height" overflow.
#if 0
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

