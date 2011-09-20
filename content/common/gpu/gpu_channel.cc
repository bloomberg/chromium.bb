// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/common/gpu/gpu_channel.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "content/common/child_process.h"
#include "content/common/content_client.h"
#include "content/common/content_switches.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/transport_texture.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_surface.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

GpuChannel::GpuChannel(GpuChannelManager* gpu_channel_manager,
                       GpuWatchdog* watchdog,
                       int renderer_id,
                       bool software)
    : gpu_channel_manager_(gpu_channel_manager),
      renderer_id_(renderer_id),
      renderer_process_(base::kNullProcessHandle),
      renderer_pid_(base::kNullProcessId),
      share_group_(new gfx::GLShareGroup),
      watchdog_(watchdog),
      software_(software),
      task_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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

bool GpuChannel::OnMessageReceived(const IPC::Message& message) {
  if (log_messages_) {
    VLOG(1) << "received message @" << &message << " on channel @" << this
            << " with type " << message.type();
  }

  // Control messages are not deferred and can be handled out of order with
  // respect to routed ones.
  if (message.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(message);

  // If the channel is unscheduled, defer sync and async messages until it is
  // rescheduled. Also, even if the channel is scheduled, do not allow newly
  // received messages to be handled before previously received deferred ones;
  // append them to the deferred queue as well.
  if (!IsScheduled() || !deferred_messages_.empty()) {
    deferred_messages_.push(new IPC::Message(message));
    return true;
  }

  if (!router_.RouteMessage(message)) {
    // Respond to sync messages even if router failed to route.
    if (message.is_sync()) {
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
      reply->set_reply_error();
      Send(reply);
    }
    return false;
  }

  // If the channel becomes unscheduled as a result of handling the message,
  // synthesize an IPC message to flush the command buffer that became
  // unscheduled.
  for (StubMap::Iterator<GpuCommandBufferStub> it(&stubs_);
       !it.IsAtEnd();
       it.Advance()) {
    GpuCommandBufferStub* stub = it.GetCurrentValue();
    if (!stub->IsScheduled()) {
      DCHECK(deferred_messages_.empty());
      deferred_messages_.push(new GpuCommandBufferMsg_Rescheduled(
          stub->route_id()));
    }
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
  // The GPU process must never send a synchronous IPC message to the renderer
  // process. This could result in deadlock.
  DCHECK(!message->is_sync());
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

bool GpuChannel::IsScheduled() {
  for (StubMap::Iterator<GpuCommandBufferStub> it(&stubs_);
       !it.IsAtEnd();
       it.Advance()) {
    GpuCommandBufferStub* stub = it.GetCurrentValue();
    if (!stub->IsScheduled())
      return false;
  }

  return true;
}

void GpuChannel::OnScheduled() {
  // Post a task to handle any deferred messages. The deferred message queue is
  // not emptied here, which ensures that OnMessageReceived will continue to
  // defer newly received messages until the ones in the queue have all been
  // handled by HandleDeferredMessages. HandleDeferredMessages is invoked as a
  // task to prevent reentrancy.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      task_factory_.NewRunnableMethod(
          &GpuChannel::HandleDeferredMessages));
}

void GpuChannel::LoseAllContexts() {
  gpu_channel_manager_->LoseAllContexts();
}

void GpuChannel::DestroySoon() {
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(this,
          &GpuChannel::OnDestroy));
}

void GpuChannel::OnDestroy() {
  TRACE_EVENT0("gpu", "GpuChannel::OnDestroy");
  gpu_channel_manager_->RemoveChannel(renderer_id_);
}

void GpuChannel::CreateViewCommandBuffer(
    gfx::PluginWindowHandle window,
    int32 render_view_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32* route_id) {
  *route_id = MSG_ROUTING_NONE;
  content::GetContentClient()->SetActiveURL(init_params.active_url);

#if defined(ENABLE_GPU)
  GpuCommandBufferStub* share_group = stubs_.Lookup(init_params.share_group_id);

  *route_id = GenerateRouteID();
  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this,
      share_group,
      window,
      gfx::Size(),
      disallowed_extensions_,
      init_params.allowed_extensions,
      init_params.attribs,
      *route_id,
      renderer_id_,
      render_view_id,
      watchdog_,
      software_));
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

GpuCommandBufferStub* GpuChannel::LookupCommandBuffer(int32 route_id) {
  return stubs_.Lookup(route_id);
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
      OnDestroyCommandBuffer(iter.GetCurrentKey(), NULL);
      return;
    }
  }
}
#endif

bool GpuChannel::OnControlMessageReceived(const IPC::Message& msg) {
  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannel, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuChannelMsg_CreateOffscreenCommandBuffer,
                                    OnCreateOffscreenCommandBuffer)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuChannelMsg_DestroyCommandBuffer,
                                    OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateTransportTexture,
        OnCreateTransportTexture)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_Echo, OnEcho);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << msg.type();
  return handled;
}

void GpuChannel::HandleDeferredMessages() {
  // Empty the deferred queue so OnMessageRecieved does not defer on that
  // account and to prevent an infinite loop if the scheduler is unscheduled
  // as a result of handling already deferred messages.
  std::queue<IPC::Message*> deferred_messages_copy;
  std::swap(deferred_messages_copy, deferred_messages_);

  while (!deferred_messages_copy.empty()) {
    scoped_ptr<IPC::Message> message(deferred_messages_copy.front());
    deferred_messages_copy.pop();

    OnMessageReceived(*message);
  }
}

int GpuChannel::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

void GpuChannel::AddRoute(int32 route_id, IPC::Channel::Listener* listener) {
  router_.AddRoute(route_id, listener);
}

void GpuChannel::RemoveRoute(int32 route_id) {
  router_.RemoveRoute(route_id);
}

void GpuChannel::OnInitialize(base::ProcessHandle renderer_process) {
  // Initialize should only happen once.
  DCHECK(!renderer_process_);

  // Verify that the renderer has passed its own process handle.
  if (base::GetProcId(renderer_process) == renderer_pid_)
    renderer_process_ = renderer_process;
}

void GpuChannel::OnCreateOffscreenCommandBuffer(
    const gfx::Size& size,
    const GPUCreateCommandBufferConfig& init_params,
    IPC::Message* reply_message) {
  int32 route_id = MSG_ROUTING_NONE;

  content::GetContentClient()->SetActiveURL(init_params.active_url);
#if defined(ENABLE_GPU)
  GpuCommandBufferStub* share_group = stubs_.Lookup(init_params.share_group_id);

  route_id = GenerateRouteID();

  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this,
      share_group,
      gfx::kNullPluginWindow,
      size,
      disallowed_extensions_,
      init_params.allowed_extensions,
      init_params.attribs,
      route_id,
      0, 0, watchdog_,
      software_));
  router_.AddRoute(route_id, stub.get());
  stubs_.AddWithID(stub.release(), route_id);
  TRACE_EVENT1("gpu", "GpuChannel::OnCreateOffscreenCommandBuffer",
               "route_id", route_id);
#endif

  GpuChannelMsg_CreateOffscreenCommandBuffer::WriteReplyParams(
      reply_message,
      route_id);
  Send(reply_message);
}

void GpuChannel::OnDestroyCommandBuffer(int32 route_id,
                                        IPC::Message* reply_message) {
#if defined(ENABLE_GPU)
  TRACE_EVENT1("gpu", "GpuChannel::OnDestroyCommandBuffer",
               "route_id", route_id);
  if (router_.ResolveRoute(route_id)) {
    GpuCommandBufferStub* stub = stubs_.Lookup(route_id);
    bool need_reschedule = (stub && !stub->IsScheduled());
    router_.RemoveRoute(route_id);
    stubs_.Remove(route_id);
    // In case the renderer is currently blocked waiting for a sync reply from
    // the stub, we need to make sure to reschedule the GpuChannel here.
    if (need_reschedule)
      OnScheduled();
  }
#endif

  if (reply_message)
    Send(reply_message);
}

void GpuChannel::OnCreateTransportTexture(int32 context_route_id,
                                          int32 host_id) {
 #if defined(ENABLE_GPU)
   GpuCommandBufferStub* stub = stubs_.Lookup(context_route_id);
   int32 route_id = GenerateRouteID();

   scoped_ptr<TransportTexture> transport(
       new TransportTexture(this, channel_.get(), stub->decoder(),
                            host_id, route_id));
   router_.AddRoute(route_id, transport.get());
   transport_textures_.AddWithID(transport.release(), route_id);

   IPC::Message* msg = new GpuTransportTextureHostMsg_TransportTextureCreated(
       host_id, route_id);
   Send(msg);
 #endif
}

void GpuChannel::OnEcho(const IPC::Message& message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnEcho");
  Send(new IPC::Message(message));
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
int GpuChannel::TakeRendererFileDescriptor() {
  if (!channel_.get()) {
    NOTREACHED();
    return -1;
  }
  return channel_->TakeClientFileDescriptor();
}
#endif  // defined(OS_POSIX)
