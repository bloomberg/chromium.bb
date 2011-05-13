// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/common/gpu/gpu_channel.h"

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "content/common/child_process.h"
#include "content/common/content_client.h"
#include "content/common/content_switches.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_video_service.h"
#include "content/common/gpu/transport_texture.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

GpuChannel::GpuChannel(GpuChannelManager* gpu_channel_manager,
                       GpuWatchdog* watchdog,
                       int renderer_id)
    : gpu_channel_manager_(gpu_channel_manager),
      renderer_id_(renderer_id),
      renderer_process_(base::kNullProcessHandle),
      renderer_pid_(base::kNullProcessId),
      watchdog_(watchdog) {
  DCHECK(gpu_channel_manager);
  DCHECK(renderer_id);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);
  disallowed_extensions_.multisampling =
      command_line->HasSwitch(switches::kDisableGLMultisampling);
}

GpuChannel::~GpuChannel() {
#if defined(OS_WIN)
  if (renderer_process_)
    CloseHandle(renderer_process_);
#endif
}

TransportTexture* GpuChannel::GetTransportTexture(int32 route_id) {
  return transport_textures_.Lookup(route_id);
}

void GpuChannel::DestroyTransportTexture(int32 route_id) {
  transport_textures_.Remove(route_id);
  router_.RemoveRoute(route_id);
}

void GpuChannel::OnLatchCallback(int route_id, bool is_set_latch) {
#if defined(ENABLE_GPU)
  if (is_set_latch) {
    // Wake up any waiting contexts. If they are still blocked, they will re-add
    // themselves to the set.
    for (std::set<int32>::iterator i = latched_routes_.begin();
         i != latched_routes_.end(); ++i) {
      GpuCommandBufferStub* stub = stubs_.Lookup(*i);
      if (stub)
        stub->scheduler()->SetScheduled(true);
    }
    latched_routes_.clear();
  } else {
    // Add route_id context to a set to be woken upon any set latch.
    latched_routes_.insert(route_id);
    GpuCommandBufferStub* stub = stubs_.Lookup(route_id);
    if (stub)
      stub->scheduler()->SetScheduled(false);
  }
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
  gpu_channel_manager_->RemoveChannel(renderer_id_);
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

void GpuChannel::LoseAllContexts() {
  gpu_channel_manager_->LoseAllContexts();
}

void GpuChannel::CreateViewCommandBuffer(
    gfx::PluginWindowHandle window,
    int32 render_view_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32* route_id) {
  *route_id = MSG_ROUTING_NONE;
  content::GetContentClient()->SetActiveURL(init_params.active_url);

#if defined(ENABLE_GPU)
  *route_id = GenerateRouteID();
  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this, window, NULL, gfx::Size(), disallowed_extensions_,
      init_params.allowed_extensions,
      init_params.attribs, 0, *route_id, renderer_id_, render_view_id,
      watchdog_));
  router_.AddRoute(*route_id, stub.get());
  stubs_.AddWithID(stub.release(), *route_id);
#endif  // ENABLE_GPU
}

void GpuChannel::ViewResized(int32 command_buffer_route_id) {
  GpuCommandBufferStub* stub = stubs_.Lookup(command_buffer_route_id);
  if (stub == NULL)
    return;

  stub->ViewResized();
}

#if defined(OS_MACOSX)
void GpuChannel::AcceleratedSurfaceBuffersSwapped(
    int32 route_id, uint64 swap_buffers_count) {
  GpuCommandBufferStub* stub = stubs_.Lookup(route_id);
  if (stub == NULL)
    return;
  stub->AcceleratedSurfaceBuffersSwapped(swap_buffers_count);
}

void GpuChannel::DestroyCommandBufferByViewId(int32 render_view_id) {
  // This responds to a message from the browser process to destroy the command
  // buffer when the window with a GpuScheduler is closed (see
  // RenderWidgetHostViewMac::DeallocFakePluginWindowHandle).  Find the route id
  // that matches the given render_view_id and delete the route.
  for (StubMap::const_iterator iter(&stubs_); !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrentValue()->render_view_id() == render_view_id) {
      OnDestroyCommandBuffer(iter.GetCurrentKey());
      return;
    }
  }
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
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateTransportTexture,
        OnCreateTransportTexture)
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
  content::GetContentClient()->SetActiveURL(init_params.active_url);
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
      disallowed_extensions_,
      init_params.allowed_extensions,
      init_params.attribs,
      parent_texture_id,
      *route_id,
      0, 0, watchdog_));
  router_.AddRoute(*route_id, stub.get());
  stubs_.AddWithID(stub.release(), *route_id);
#else
  *route_id = MSG_ROUTING_NONE;
#endif
}

void GpuChannel::OnDestroyCommandBuffer(int32 route_id) {
#if defined(ENABLE_GPU)
  if (router_.ResolveRoute(route_id)) {
    router_.RemoveRoute(route_id);
    stubs_.Remove(route_id);
  }
#endif
}

void GpuChannel::OnCreateVideoDecoder(
    int32 decoder_host_id, const std::vector<uint32>& configs) {
// TODO(cevans): do NOT re-enable this until GpuVideoService has been checked
// for integer overflows, including the classic "width * height" overflow.
#if 0
  VLOG(1) << "GpuChannel::OnCreateVideoDecoder";
  GpuVideoService* service = GpuVideoService::GetInstance();
  if (service == NULL) {
    // TODO(hclam): Need to send a failure message.
    return;
  }

  int32 decoder_id = GenerateRouteID();

  bool ret = service->CreateVideoDecoder(
      this, &router_, decoder_host_id, decoder_id, configs);
  DCHECK(ret) << "Failed to create a GpuVideoDecodeAccelerator";
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

void GpuChannel::OnCreateTransportTexture(int32 context_route_id,
                                          int32 host_id) {
 #if defined(ENABLE_GPU)
   GpuCommandBufferStub* stub = stubs_.Lookup(context_route_id);
   int32 route_id = GenerateRouteID();

   scoped_ptr<TransportTexture> transport(
       new TransportTexture(this, channel_.get(), stub->scheduler()->decoder(),
                            host_id, route_id));
   router_.AddRoute(route_id, transport.get());
   transport_textures_.AddWithID(transport.release(), route_id);
 
   IPC::Message* msg = new GpuTransportTextureHostMsg_TransportTextureCreated(
       host_id, route_id);
   Send(msg);
 #endif
 }

bool GpuChannel::Init(base::MessageLoopProxy* io_message_loop,
                      base::WaitableEvent* shutdown_event) {
  // Check whether we're already initialized.
  if (channel_.get())
    return true;

  // Map renderer ID to a (single) channel to that process.
  std::string channel_name = GetChannelName();
  channel_.reset(new IPC::SyncChannel(
      channel_name,
      IPC::Channel::MODE_SERVER,
      this,
      io_message_loop,
      false,
      shutdown_event));

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
