// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/common/gpu/gpu_channel.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "content/common/child_process.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

namespace {
// The first time polling a fence, delay some extra time to allow other
// stubs to process some work, or else the timing of the fences could
// allow a pattern of alternating fast and slow frames to occur.
const int64 kHandleMoreWorkPeriodMs = 2;
const int64 kHandleMoreWorkPeriodBusyMs = 1;
}

GpuChannel::GpuChannel(GpuChannelManager* gpu_channel_manager,
                       GpuWatchdog* watchdog,
                       gfx::GLShareGroup* share_group,
                       int client_id,
                       bool software)
    : gpu_channel_manager_(gpu_channel_manager),
      client_id_(client_id),
      share_group_(share_group ? share_group : new gfx::GLShareGroup),
      mailbox_manager_(new gpu::gles2::MailboxManager),
      watchdog_(watchdog),
      software_(software),
      handle_messages_scheduled_(false),
      processed_get_state_fast_(false),
      num_contexts_preferring_discrete_gpu_(0),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(gpu_channel_manager);
  DCHECK(client_id);

  channel_id_ = IPC::Channel::GenerateVerifiedChannelID("gpu");
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);
  disallowed_features_.multisampling =
      command_line->HasSwitch(switches::kDisableGLMultisampling);
}


bool GpuChannel::Init(base::MessageLoopProxy* io_message_loop,
                      base::WaitableEvent* shutdown_event) {
  DCHECK(!channel_.get());

  // Map renderer ID to a (single) channel to that process.
  channel_.reset(new IPC::SyncChannel(
      channel_id_,
      IPC::Channel::MODE_SERVER,
      this,
      io_message_loop,
      false,
      shutdown_event));

  return true;
}

std::string GpuChannel::GetChannelName() {
  return channel_id_;
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

bool GpuChannel::OnMessageReceived(const IPC::Message& message) {
  if (log_messages_) {
    DVLOG(1) << "received message @" << &message << " on channel @" << this
             << " with type " << message.type();
  }

  // Control messages are not deferred and can be handled out of order with
  // respect to routed ones.
  if (message.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(message);

  if (message.type() == GpuCommandBufferMsg_GetStateFast::ID) {
    if (processed_get_state_fast_) {
      // Require a non-GetStateFast message in between two GetStateFast
      // messages, to ensure progress is made.
      std::deque<IPC::Message*>::iterator point = deferred_messages_.begin();

      while (point != deferred_messages_.end() &&
             (*point)->type() == GpuCommandBufferMsg_GetStateFast::ID) {
        ++point;
      }

      if (point != deferred_messages_.end()) {
        ++point;
      }

      deferred_messages_.insert(point, new IPC::Message(message));
    } else {
      // Move GetStateFast commands to the head of the queue, so the renderer
      // doesn't have to wait any longer than necessary.
      deferred_messages_.push_front(new IPC::Message(message));
    }
  } else {
    deferred_messages_.push_back(new IPC::Message(message));
  }

  OnScheduled();

  return true;
}

void GpuChannel::OnChannelError() {
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::Send(IPC::Message* message) {
  // The GPU process must never send a synchronous IPC message to the renderer
  // process. This could result in deadlock.
  DCHECK(!message->is_sync());
  if (log_messages_) {
    DVLOG(1) << "sending message @" << message << " on channel @" << this
             << " with type " << message->type();
  }

  if (!channel_.get()) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void GpuChannel::AppendAllCommandBufferStubs(
    std::vector<GpuCommandBufferStubBase*>& stubs) {
  for (StubMap::Iterator<GpuCommandBufferStub> it(&stubs_);
      !it.IsAtEnd(); it.Advance()) {
    stubs.push_back(it.GetCurrentValue());
  }
}

void GpuChannel::OnScheduled() {
  if (handle_messages_scheduled_)
    return;
  // Post a task to handle any deferred messages. The deferred message queue is
  // not emptied here, which ensures that OnMessageReceived will continue to
  // defer newly received messages until the ones in the queue have all been
  // handled by HandleMessage. HandleMessage is invoked as a
  // task to prevent reentrancy.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GpuChannel::HandleMessage, weak_factory_.GetWeakPtr()));
  handle_messages_scheduled_ = true;
}

void GpuChannel::CreateViewCommandBuffer(
    const gfx::GLSurfaceHandle& window,
    int32 surface_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32* route_id) {
  TRACE_EVENT1("gpu",
               "GpuChannel::CreateViewCommandBuffer",
               "surface_id",
               surface_id);

  *route_id = MSG_ROUTING_NONE;
  content::GetContentClient()->SetActiveURL(init_params.active_url);

#if defined(ENABLE_GPU)
  WillCreateCommandBuffer(init_params.gpu_preference);

  GpuCommandBufferStub* share_group = stubs_.Lookup(init_params.share_group_id);

  *route_id = GenerateRouteID();
  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this,
      share_group,
      window,
      mailbox_manager_,
      gfx::Size(),
      disallowed_features_,
      init_params.allowed_extensions,
      init_params.attribs,
      init_params.gpu_preference,
      *route_id,
      surface_id,
      watchdog_,
      software_));
  router_.AddRoute(*route_id, stub.get());
  stubs_.AddWithID(stub.release(), *route_id);
#endif  // ENABLE_GPU
}

GpuCommandBufferStub* GpuChannel::LookupCommandBuffer(int32 route_id) {
  return stubs_.Lookup(route_id);
}

void GpuChannel::LoseAllContexts() {
  gpu_channel_manager_->LoseAllContexts();
}

void GpuChannel::DestroySoon() {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&GpuChannel::OnDestroy, this));
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

bool GpuChannel::ShouldPreferDiscreteGpu() const {
  return num_contexts_preferring_discrete_gpu_ > 0;
}

GpuChannel::~GpuChannel() {}

void GpuChannel::OnDestroy() {
  TRACE_EVENT0("gpu", "GpuChannel::OnDestroy");
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::OnControlMessageReceived(const IPC::Message& msg) {
  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannel, msg)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuChannelMsg_CreateOffscreenCommandBuffer,
                                    OnCreateOffscreenCommandBuffer)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuChannelMsg_DestroyCommandBuffer,
                                    OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuChannelMsg_WillGpuSwitchOccur,
                                    OnWillGpuSwitchOccur)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CloseChannel, OnCloseChannel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << msg.type();
  return handled;
}

void GpuChannel::HandleMessage() {
  handle_messages_scheduled_ = false;

  if (!deferred_messages_.empty()) {
    IPC::Message* m = deferred_messages_.front();
    GpuCommandBufferStub* stub = stubs_.Lookup(m->routing_id());

    if (stub && !stub->IsScheduled()) {
      if (m->type() == GpuCommandBufferMsg_Echo::ID) {
        stub->DelayEcho(m);
        deferred_messages_.pop_front();
        if (!deferred_messages_.empty())
          OnScheduled();
      }
      return;
    }

    scoped_ptr<IPC::Message> message(m);
    deferred_messages_.pop_front();
    processed_get_state_fast_ =
        (message->type() == GpuCommandBufferMsg_GetStateFast::ID);
    // Handle deferred control messages.
    if (!router_.RouteMessage(*message)) {
      // Respond to sync messages even if router failed to route.
      if (message->is_sync()) {
        IPC::Message* reply = IPC::SyncMessage::GenerateReply(&*message);
        reply->set_reply_error();
        Send(reply);
      }
    } else {
      // If the command buffer becomes unscheduled as a result of handling the
      // message but still has more commands to process, synthesize an IPC
      // message to flush that command buffer.
      if (stub) {
        if (stub->HasUnprocessedCommands()) {
          deferred_messages_.push_front(new GpuCommandBufferMsg_Rescheduled(
              stub->route_id()));
        }

        ScheduleDelayedWork(stub, kHandleMoreWorkPeriodMs);
      }
    }
  }

  if (!deferred_messages_.empty()) {
    OnScheduled();
  }
}

void GpuChannel::PollWork(int route_id) {
  GpuCommandBufferStub* stub = stubs_.Lookup(route_id);
  if (stub) {
    stub->PollWork();

    ScheduleDelayedWork(stub, kHandleMoreWorkPeriodBusyMs);
  }
}

void GpuChannel::ScheduleDelayedWork(GpuCommandBufferStub *stub,
                                     int64 delay) {
  if (stub->HasMoreWork()) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GpuChannel::PollWork,
                   weak_factory_.GetWeakPtr(),
                   stub->route_id()),
        base::TimeDelta::FromMilliseconds(delay));
  }
}

void GpuChannel::OnCreateOffscreenCommandBuffer(
    const gfx::Size& size,
    const GPUCreateCommandBufferConfig& init_params,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuChannel::OnCreateOffscreenCommandBuffer");

  int32 route_id = MSG_ROUTING_NONE;

  content::GetContentClient()->SetActiveURL(init_params.active_url);
#if defined(ENABLE_GPU)
  WillCreateCommandBuffer(init_params.gpu_preference);

  GpuCommandBufferStub* share_group = stubs_.Lookup(init_params.share_group_id);

  route_id = GenerateRouteID();

  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this,
      share_group,
      gfx::GLSurfaceHandle(),
      mailbox_manager_.get(),
      size,
      disallowed_features_,
      init_params.allowed_extensions,
      init_params.attribs,
      init_params.gpu_preference,
      route_id,
      0, watchdog_,
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
  TRACE_EVENT1("gpu", "GpuChannel::OnDestroyCommandBuffer",
               "route_id", route_id);

  if (router_.ResolveRoute(route_id)) {
    GpuCommandBufferStub* stub = stubs_.Lookup(route_id);
    bool need_reschedule = (stub && !stub->IsScheduled());
    gfx::GpuPreference gpu_preference =
        stub ? stub->gpu_preference() : gfx::PreferIntegratedGpu;
    router_.RemoveRoute(route_id);
    stubs_.Remove(route_id);
    // In case the renderer is currently blocked waiting for a sync reply from
    // the stub, we need to make sure to reschedule the GpuChannel here.
    if (need_reschedule)
      OnScheduled();
    DidDestroyCommandBuffer(gpu_preference);
  }

  if (reply_message)
    Send(reply_message);
}


void GpuChannel::OnWillGpuSwitchOccur(bool is_creating_context,
                                      gfx::GpuPreference gpu_preference,
                                      IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuChannel::OnWillGpuSwitchOccur");

  bool will_switch_occur = false;

  if (gpu_preference == gfx::PreferDiscreteGpu &&
      gfx::GLContext::SupportsDualGpus()) {
    if (is_creating_context) {
      will_switch_occur = !num_contexts_preferring_discrete_gpu_;
    } else {
      will_switch_occur = (num_contexts_preferring_discrete_gpu_ == 1);
    }
  }

  GpuChannelMsg_WillGpuSwitchOccur::WriteReplyParams(
      reply_message,
      will_switch_occur);
  Send(reply_message);
}

void GpuChannel::OnCloseChannel() {
  gpu_channel_manager_->RemoveChannel(client_id_);
  // At this point "this" is deleted!
}

void GpuChannel::WillCreateCommandBuffer(gfx::GpuPreference gpu_preference) {
  if (gpu_preference == gfx::PreferDiscreteGpu)
    ++num_contexts_preferring_discrete_gpu_;
}

void GpuChannel::DidDestroyCommandBuffer(gfx::GpuPreference gpu_preference) {
  if (gpu_preference == gfx::PreferDiscreteGpu)
    --num_contexts_preferring_discrete_gpu_;
  DCHECK_GE(num_contexts_preferring_discrete_gpu_, 0);
}
