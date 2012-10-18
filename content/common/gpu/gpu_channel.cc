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
#include "base/message_loop_proxy.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "content/common/child_process.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/sync_point_manager.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/stream_texture_manager_android.h"
#endif

namespace {
// This filter does two things:
// - it counts the number of messages coming in on the channel
// - it handles the GpuCommandBufferMsg_InsertSyncPoint message on the IO
//   thread, generating the sync point ID and responding immediately, and then
//   posting a task to insert the GpuCommandBufferMsg_RetireSyncPoint message
//   into the channel's queue.
class SyncPointMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  // Takes ownership of gpu_channel (see below).
  SyncPointMessageFilter(base::WeakPtr<GpuChannel>* gpu_channel,
                         scoped_refptr<SyncPointManager> sync_point_manager,
                         scoped_refptr<base::MessageLoopProxy> message_loop,
                         scoped_refptr<gpu::RefCountedCounter>
                             unprocessed_messages)
      : gpu_channel_(gpu_channel),
        channel_(NULL),
        sync_point_manager_(sync_point_manager),
        message_loop_(message_loop),
        unprocessed_messages_(unprocessed_messages) {
  }

  virtual void OnFilterAdded(IPC::Channel* channel) {
    DCHECK(!channel_);
    channel_ = channel;
  }

  virtual void OnFilterRemoved() {
    DCHECK(channel_);
    channel_ = NULL;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) {
    DCHECK(channel_);
    unprocessed_messages_->IncCount();
    if (message.type() == GpuCommandBufferMsg_InsertSyncPoint::ID) {
      uint32 sync_point = sync_point_manager_->GenerateSyncPoint();
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
      GpuCommandBufferMsg_InsertSyncPoint::WriteReplyParams(reply, sync_point);
      channel_->Send(reply);
      message_loop_->PostTask(FROM_HERE, base::Bind(
          &SyncPointMessageFilter::InsertSyncPointOnMainThread,
          gpu_channel_,
          sync_point_manager_,
          message.routing_id(),
          sync_point,
          unprocessed_messages_));
      return true;
    } else if (message.type() == GpuCommandBufferMsg_RetireSyncPoint::ID) {
      // This message should not be sent explicitly by the renderer.
      NOTREACHED();
      unprocessed_messages_->DecCount();
      return true;
    } else {
      return false;
    }
  }

 protected:
  virtual ~SyncPointMessageFilter() {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &SyncPointMessageFilter::DeleteWeakPtrOnMainThread, gpu_channel_));
  }

 private:
  static void InsertSyncPointOnMainThread(
      base::WeakPtr<GpuChannel>* gpu_channel,
      scoped_refptr<SyncPointManager> manager,
      int32 routing_id,
      uint32 sync_point,
      scoped_refptr<gpu::RefCountedCounter> unprocessed_messages_) {
    // This function must ensure that the sync point will be retired. Normally
    // we'll find the stub based on the routing ID, and associate the sync point
    // with it, but if that fails for any reason (channel or stub already
    // deleted, invalid routing id), we need to retire the sync point
    // immediately.
    if (gpu_channel->get()) {
      GpuCommandBufferStub* stub = gpu_channel->get()->LookupCommandBuffer(
          routing_id);
      if (stub) {
        stub->AddSyncPoint(sync_point);
        GpuCommandBufferMsg_RetireSyncPoint message(routing_id, sync_point);
        gpu_channel->get()->OnMessageReceived(message);
        return;
      } else {
        unprocessed_messages_->DecCount();
      }
    }
    manager->RetireSyncPoint(sync_point);
  }

  static void DeleteWeakPtrOnMainThread(
      base::WeakPtr<GpuChannel>* gpu_channel) {
    delete gpu_channel;
  }

  // NOTE: this is a pointer to a weak pointer. It is never dereferenced on the
  // IO thread, it's only passed through - therefore the WeakPtr assumptions are
  // respected.
  base::WeakPtr<GpuChannel>* gpu_channel_;
  IPC::Channel* channel_;
  scoped_refptr<SyncPointManager> sync_point_manager_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  scoped_refptr<gpu::RefCountedCounter> unprocessed_messages_;
};

}  // anonymous namespace

GpuChannel::GpuChannel(GpuChannelManager* gpu_channel_manager,
                       GpuWatchdog* watchdog,
                       gfx::GLShareGroup* share_group,
                       gpu::gles2::MailboxManager* mailbox,
                       int client_id,
                       bool software)
    : gpu_channel_manager_(gpu_channel_manager),
      unprocessed_messages_(new gpu::RefCountedCounter),
      client_id_(client_id),
      share_group_(share_group ? share_group : new gfx::GLShareGroup),
      mailbox_manager_(mailbox ? mailbox : new gpu::gles2::MailboxManager),
      watchdog_(watchdog),
      software_(software),
      handle_messages_scheduled_(false),
      processed_get_state_fast_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(gpu_channel_manager);
  DCHECK(client_id);

  channel_id_ = IPC::Channel::GenerateVerifiedChannelID("gpu");
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);
  disallowed_features_.multisampling =
      command_line->HasSwitch(switches::kDisableGLMultisampling);
#if defined(OS_ANDROID)
  stream_texture_manager_.reset(new content::StreamTextureManagerAndroid(this));
#endif
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

  base::WeakPtr<GpuChannel>* weak_ptr(new base::WeakPtr<GpuChannel>(
      weak_factory_.GetWeakPtr()));
  scoped_refptr<SyncPointMessageFilter> filter(new SyncPointMessageFilter(
      weak_ptr,
      gpu_channel_manager_->sync_point_manager(),
      base::MessageLoopProxy::current(),
      unprocessed_messages_));
  channel_->AddFilter(filter);

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
  // Drop the count that was incremented on the IO thread because we are
  // about to process that message.
  unprocessed_messages_->DecCount();
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
      unprocessed_messages_->IncCount();
    } else {
      // Move GetStateFast commands to the head of the queue, so the renderer
      // doesn't have to wait any longer than necessary.
      deferred_messages_.push_front(new IPC::Message(message));
      unprocessed_messages_->IncCount();
    }
  } else {
    deferred_messages_.push_back(new IPC::Message(message));
    unprocessed_messages_->IncCount();
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

#if defined(ENABLE_GPU)

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
      software_,
      init_params.active_url));
  if (preempt_by_counter_.get())
    stub->SetPreemptByCounter(preempt_by_counter_);
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

void GpuChannel::AddRoute(int32 route_id, IPC::Listener* listener) {
  router_.AddRoute(route_id, listener);
}

void GpuChannel::RemoveRoute(int32 route_id) {
  router_.RemoveRoute(route_id);
}

void GpuChannel::SetPreemptByCounter(
    scoped_refptr<gpu::RefCountedCounter> preempt_by_counter) {
  preempt_by_counter_ = preempt_by_counter;

  for (StubMap::Iterator<GpuCommandBufferStub> it(&stubs_);
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->SetPreemptByCounter(preempt_by_counter_);
  }
}

GpuChannel::~GpuChannel() {
  unprocessed_messages_->Reset();
}

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
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_RegisterStreamTextureProxy,
                        OnRegisterStreamTextureProxy)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_EstablishStreamTexture,
                        OnEstablishStreamTexture)
#endif
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        GpuChannelMsg_CollectRenderingStatsForSurface,
        OnCollectRenderingStatsForSurface)
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
        unprocessed_messages_->DecCount();
        if (!deferred_messages_.empty())
          OnScheduled();
      }
      return;
    }

    scoped_ptr<IPC::Message> message(m);
    deferred_messages_.pop_front();
    unprocessed_messages_->DecCount();

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
          unprocessed_messages_->IncCount();
        }
      }
    }
  }

  if (!deferred_messages_.empty()) {
    OnScheduled();
  }
}

void GpuChannel::OnCreateOffscreenCommandBuffer(
    const gfx::Size& size,
    const GPUCreateCommandBufferConfig& init_params,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuChannel::OnCreateOffscreenCommandBuffer");

  int32 route_id = MSG_ROUTING_NONE;

#if defined(ENABLE_GPU)
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
      software_,
      init_params.active_url));
  if (preempt_by_counter_.get())
    stub->SetPreemptByCounter(preempt_by_counter_);
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
    router_.RemoveRoute(route_id);
    stubs_.Remove(route_id);
    // In case the renderer is currently blocked waiting for a sync reply from
    // the stub, we need to make sure to reschedule the GpuChannel here.
    if (need_reschedule)
      OnScheduled();
  }

  if (reply_message)
    Send(reply_message);
}

#if defined(OS_ANDROID)
void GpuChannel::OnRegisterStreamTextureProxy(
    int32 stream_id,  const gfx::Size& initial_size, int32* route_id) {
  // Note that route_id is only used for notifications sent out from here.
  // StreamTextureManager owns all texture objects and for incoming messages
  // it finds the correct object based on stream_id.
  *route_id = GenerateRouteID();
  stream_texture_manager_->RegisterStreamTextureProxy(
      stream_id, initial_size, *route_id);
}

void GpuChannel::OnEstablishStreamTexture(
    int32 stream_id, content::SurfaceTexturePeer::SurfaceTextureTarget type,
    int32 primary_id, int32 secondary_id) {
  stream_texture_manager_->EstablishStreamTexture(
      stream_id, type, primary_id, secondary_id);
}
#endif

void GpuChannel::OnCollectRenderingStatsForSurface(
    int32 surface_id, IPC::Message* reply_message) {
  content::GpuRenderingStats stats;

  for (StubMap::Iterator<GpuCommandBufferStub> it(&stubs_);
       !it.IsAtEnd(); it.Advance()) {
    int texture_upload_count =
        it.GetCurrentValue()->decoder()->GetTextureUploadCount();
    base::TimeDelta total_texture_upload_time =
        it.GetCurrentValue()->decoder()->GetTotalTextureUploadTime();
    base::TimeDelta total_processing_commands_time =
        it.GetCurrentValue()->decoder()->GetTotalProcessingCommandsTime();

    stats.global_texture_upload_count += texture_upload_count;
    stats.global_total_texture_upload_time += total_texture_upload_time;
    stats.global_total_processing_commands_time +=
        total_processing_commands_time;
    if (it.GetCurrentValue()->surface_id() == surface_id) {
      stats.texture_upload_count += texture_upload_count;
      stats.total_texture_upload_time += total_texture_upload_time;
      stats.total_processing_commands_time += total_processing_commands_time;
    }
  }

  GpuChannelMsg_CollectRenderingStatsForSurface::WriteReplyParams(
      reply_message,
      stats);
  Send(reply_message);
}
