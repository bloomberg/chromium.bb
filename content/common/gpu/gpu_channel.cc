// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "content/common/gpu/gpu_channel.h"

#include <queue>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop_proxy.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/timer.h"
#include "content/common/child_process.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/sync_point_manager.h"
#include "content/public/common/content_switches.h"
#include "crypto/hmac.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/stream_texture_manager_android.h"
#endif

namespace content {
namespace {

// Number of milliseconds between successive vsync. Many GL commands block
// on vsync, so thresholds for preemption should be multiples of this.
const int64 kVsyncIntervalMs = 17;

// Amount of time that we will wait for an IPC to be processed before
// preempting. After a preemption, we must wait this long before triggering
// another preemption.
const int64 kPreemptWaitTimeMs = 2 * kVsyncIntervalMs;

// Once we trigger a preemption, the maximum duration that we will wait
// before clearing the preemption.
const int64 kMaxPreemptTimeMs = kVsyncIntervalMs;

// Stop the preemption once the time for the longest pending IPC drops
// below this threshold.
const int64 kStopPreemptThresholdMs = kVsyncIntervalMs;

}  // anonymous namespace

// This filter does three things:
// - it counts and timestamps each message forwarded to the channel
//   so that we can preempt other channels if a message takes too long to
//   process. To guarantee fairness, we must wait a minimum amount of time
//   before preempting and we limit the amount of time that we can preempt in
//   one shot (see constants above).
// - it handles the GpuCommandBufferMsg_InsertSyncPoint message on the IO
//   thread, generating the sync point ID and responding immediately, and then
//   posting a task to insert the GpuCommandBufferMsg_RetireSyncPoint message
//   into the channel's queue.
// - it generates mailbox names for clients of the GPU process on the IO thread.
class GpuChannelMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  // Takes ownership of gpu_channel (see below).
  GpuChannelMessageFilter(const std::string& private_key,
                          base::WeakPtr<GpuChannel>* gpu_channel,
                          scoped_refptr<SyncPointManager> sync_point_manager,
                          scoped_refptr<base::MessageLoopProxy> message_loop)
      : preemption_state_(IDLE),
        gpu_channel_(gpu_channel),
        channel_(NULL),
        sync_point_manager_(sync_point_manager),
        message_loop_(message_loop),
        messages_forwarded_to_channel_(0),
        a_stub_is_descheduled_(false),
        hmac_(crypto::HMAC::SHA256) {
    bool success = hmac_.Init(base::StringPiece(private_key));
    DCHECK(success);
  }

  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE {
    DCHECK(!channel_);
    channel_ = channel;
  }

  virtual void OnFilterRemoved() OVERRIDE {
    DCHECK(channel_);
    channel_ = NULL;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    DCHECK(channel_);

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(GpuChannelMessageFilter, message)
      IPC_MESSAGE_HANDLER(GpuChannelMsg_GenerateMailboxNames,
                          OnGenerateMailboxNames)
      IPC_MESSAGE_HANDLER(GpuChannelMsg_GenerateMailboxNamesAsync,
                          OnGenerateMailboxNamesAsync)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()

    if (message.type() == GpuCommandBufferMsg_RetireSyncPoint::ID) {
      // This message should not be sent explicitly by the renderer.
      NOTREACHED();
      handled = true;
    }

    // All other messages get processed by the GpuChannel.
    if (!handled) {
      messages_forwarded_to_channel_++;
      if (preempting_flag_.get())
        pending_messages_.push(PendingMessage(messages_forwarded_to_channel_));
      UpdatePreemptionState();
    }

    if (message.type() == GpuCommandBufferMsg_InsertSyncPoint::ID) {
      uint32 sync_point = sync_point_manager_->GenerateSyncPoint();
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
      GpuCommandBufferMsg_InsertSyncPoint::WriteReplyParams(reply, sync_point);
      Send(reply);
      message_loop_->PostTask(FROM_HERE, base::Bind(
          &GpuChannelMessageFilter::InsertSyncPointOnMainThread,
          gpu_channel_,
          sync_point_manager_,
          message.routing_id(),
          sync_point));
      handled = true;
    }
    return handled;
  }

  void MessageProcessed(uint64 messages_processed) {
    while (!pending_messages_.empty() &&
           pending_messages_.front().message_number <= messages_processed)
      pending_messages_.pop();
    UpdatePreemptionState();
  }

  void SetPreemptingFlagAndSchedulingState(
      gpu::PreemptionFlag* preempting_flag,
      bool a_stub_is_descheduled) {
    preempting_flag_ = preempting_flag;
    a_stub_is_descheduled_ = a_stub_is_descheduled;
  }

  void UpdateStubSchedulingState(bool a_stub_is_descheduled) {
    a_stub_is_descheduled_ = a_stub_is_descheduled;
    UpdatePreemptionState();
  }

  bool Send(IPC::Message* message) {
    return channel_->Send(message);
  }

 protected:
  virtual ~GpuChannelMessageFilter() {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuChannelMessageFilter::DeleteWeakPtrOnMainThread, gpu_channel_));
  }

 private:
  // Message handlers.
  void OnGenerateMailboxNames(unsigned num, std::vector<gpu::Mailbox>* result) {
    TRACE_EVENT1("gpu", "OnGenerateMailboxNames", "num", num);

    result->resize(num);

    for (unsigned i = 0; i < num; ++i) {
      char name[GL_MAILBOX_SIZE_CHROMIUM];
      base::RandBytes(name, sizeof(name) / 2);

      bool success = hmac_.Sign(
          base::StringPiece(name, sizeof(name) / 2),
          reinterpret_cast<unsigned char*>(name) + sizeof(name) / 2,
          sizeof(name) / 2);
      DCHECK(success);

      (*result)[i].SetName(reinterpret_cast<int8*>(name));
    }
  }

  void OnGenerateMailboxNamesAsync(unsigned num) {
    std::vector<gpu::Mailbox> names;
    OnGenerateMailboxNames(num, &names);
    Send(new GpuChannelMsg_GenerateMailboxNamesReply(names));
  }

  enum PreemptionState {
    // Either there's no other channel to preempt, there are no messages
    // pending processing, or we just finished preempting and have to wait
    // before preempting again.
    IDLE,
    // We are waiting kPreemptWaitTimeMs before checking if we should preempt.
    WAITING,
    // We can preempt whenever any IPC processing takes more than
    // kPreemptWaitTimeMs.
    CHECKING,
    // We are currently preempting (i.e. no stub is descheduled).
    PREEMPTING,
    // We would like to preempt, but some stub is descheduled.
    WOULD_PREEMPT_DESCHEDULED,
  };

  PreemptionState preemption_state_;

  // Maximum amount of time that we can spend in PREEMPTING.
  // It is reset when we transition to IDLE.
  base::TimeDelta max_preemption_time_;

  struct PendingMessage {
    uint64 message_number;
    base::TimeTicks time_received;

    explicit PendingMessage(uint64 message_number)
        : message_number(message_number),
          time_received(base::TimeTicks::Now()) {
    }
  };

  void UpdatePreemptionState() {
    switch (preemption_state_) {
      case IDLE:
        if (preempting_flag_.get() && !pending_messages_.empty())
          TransitionToWaiting();
        break;
      case WAITING:
        // A timer will transition us to CHECKING.
        DCHECK(timer_.IsRunning());
        break;
      case CHECKING:
        if (!pending_messages_.empty()) {
          base::TimeDelta time_elapsed =
              base::TimeTicks::Now() - pending_messages_.front().time_received;
          if (time_elapsed.InMilliseconds() < kPreemptWaitTimeMs) {
            // Schedule another check for when the IPC may go long.
            timer_.Start(
                FROM_HERE,
                base::TimeDelta::FromMilliseconds(kPreemptWaitTimeMs) -
                    time_elapsed,
                this, &GpuChannelMessageFilter::UpdatePreemptionState);
          } else {
            if (a_stub_is_descheduled_)
              TransitionToWouldPreemptDescheduled();
            else
              TransitionToPreempting();
          }
        }
        break;
      case PREEMPTING:
        // A TransitionToIdle() timer should always be running in this state.
        DCHECK(timer_.IsRunning());
        if (a_stub_is_descheduled_)
          TransitionToWouldPreemptDescheduled();
        else
          TransitionToIdleIfCaughtUp();
        break;
      case WOULD_PREEMPT_DESCHEDULED:
        // A TransitionToIdle() timer should never be running in this state.
        DCHECK(!timer_.IsRunning());
        if (!a_stub_is_descheduled_)
          TransitionToPreempting();
        else
          TransitionToIdleIfCaughtUp();
        break;
      default:
        NOTREACHED();
    }
  }

  void TransitionToIdleIfCaughtUp() {
    DCHECK(preemption_state_ == PREEMPTING ||
           preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
    if (pending_messages_.empty()) {
      TransitionToIdle();
    } else {
      base::TimeDelta time_elapsed =
          base::TimeTicks::Now() - pending_messages_.front().time_received;
      if (time_elapsed.InMilliseconds() < kStopPreemptThresholdMs)
        TransitionToIdle();
    }
  }

  void TransitionToIdle() {
    DCHECK(preemption_state_ == PREEMPTING ||
           preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
    // Stop any outstanding timer set to force us from PREEMPTING to IDLE.
    timer_.Stop();

    preemption_state_ = IDLE;
    preempting_flag_->Reset();
    TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 0);

    UpdatePreemptionState();
  }

  void TransitionToWaiting() {
    DCHECK_EQ(preemption_state_, IDLE);
    DCHECK(!timer_.IsRunning());

    preemption_state_ = WAITING;
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kPreemptWaitTimeMs),
        this, &GpuChannelMessageFilter::TransitionToChecking);
  }

  void TransitionToChecking() {
    DCHECK_EQ(preemption_state_, WAITING);
    DCHECK(!timer_.IsRunning());

    preemption_state_ = CHECKING;
    max_preemption_time_ = base::TimeDelta::FromMilliseconds(kMaxPreemptTimeMs);
    UpdatePreemptionState();
  }

  void TransitionToPreempting() {
    DCHECK(preemption_state_ == CHECKING ||
           preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
    DCHECK(!a_stub_is_descheduled_);

    // Stop any pending state update checks that we may have queued
    // while CHECKING.
    if (preemption_state_ == CHECKING)
      timer_.Stop();

    preemption_state_ = PREEMPTING;
    preempting_flag_->Set();
    TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 1);

    timer_.Start(
       FROM_HERE,
       max_preemption_time_,
       this, &GpuChannelMessageFilter::TransitionToIdle);

    UpdatePreemptionState();
  }

  void TransitionToWouldPreemptDescheduled() {
    DCHECK(preemption_state_ == CHECKING ||
           preemption_state_ == PREEMPTING);
    DCHECK(a_stub_is_descheduled_);

    if (preemption_state_ == CHECKING) {
      // Stop any pending state update checks that we may have queued
      // while CHECKING.
      timer_.Stop();
    } else {
      // Stop any TransitionToIdle() timers that we may have queued
      // while PREEMPTING.
      timer_.Stop();
      max_preemption_time_ = timer_.desired_run_time() - base::TimeTicks::Now();
      if (max_preemption_time_ < base::TimeDelta()) {
        TransitionToIdle();
        return;
      }
    }

    preemption_state_ = WOULD_PREEMPT_DESCHEDULED;
    preempting_flag_->Reset();
    TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 0);

    UpdatePreemptionState();
  }

  static void InsertSyncPointOnMainThread(
      base::WeakPtr<GpuChannel>* gpu_channel,
      scoped_refptr<SyncPointManager> manager,
      int32 routing_id,
      uint32 sync_point) {
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
        gpu_channel->get()->MessageProcessed();
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
  scoped_refptr<gpu::PreemptionFlag> preempting_flag_;

  std::queue<PendingMessage> pending_messages_;

  // Count of the number of IPCs forwarded to the GpuChannel.
  uint64 messages_forwarded_to_channel_;

  base::OneShotTimer<GpuChannelMessageFilter> timer_;

  bool a_stub_is_descheduled_;

  crypto::HMAC hmac_;
};

GpuChannel::GpuChannel(GpuChannelManager* gpu_channel_manager,
                       GpuWatchdog* watchdog,
                       gfx::GLShareGroup* share_group,
                       gpu::gles2::MailboxManager* mailbox,
                       int client_id,
                       bool software)
    : gpu_channel_manager_(gpu_channel_manager),
      messages_processed_(0),
      client_id_(client_id),
      share_group_(share_group ? share_group : new gfx::GLShareGroup),
      mailbox_manager_(mailbox ? mailbox : new gpu::gles2::MailboxManager),
      image_manager_(new gpu::gles2::ImageManager),
      watchdog_(watchdog),
      software_(software),
      handle_messages_scheduled_(false),
      processed_get_state_fast_(false),
      currently_processing_message_(NULL),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      num_stubs_descheduled_(0) {
  DCHECK(gpu_channel_manager);
  DCHECK(client_id);

  channel_id_ = IPC::Channel::GenerateVerifiedChannelID("gpu");
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);
  disallowed_features_.multisampling =
      command_line->HasSwitch(switches::kDisableGLMultisampling);
#if defined(OS_ANDROID)
  stream_texture_manager_.reset(new StreamTextureManagerAndroid(this));
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

  filter_ = new GpuChannelMessageFilter(
      mailbox_manager_->private_key(),
      weak_ptr,
      gpu_channel_manager_->sync_point_manager(),
      base::MessageLoopProxy::current());
  io_message_loop_ = io_message_loop;
  channel_->AddFilter(filter_);

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
  bool message_processed = true;
  if (log_messages_) {
    DVLOG(1) << "received message @" << &message << " on channel @" << this
             << " with type " << message.type();
  }

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
      message_processed = false;
    } else {
      // Move GetStateFast commands to the head of the queue, so the renderer
      // doesn't have to wait any longer than necessary.
      deferred_messages_.push_front(new IPC::Message(message));
      message_processed = false;
    }
  } else {
    deferred_messages_.push_back(new IPC::Message(message));
    message_processed = false;
  }

  if (message_processed)
    MessageProcessed();

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

void GpuChannel::RequeueMessage() {
  DCHECK(currently_processing_message_);
  deferred_messages_.push_front(
      new IPC::Message(*currently_processing_message_));
  messages_processed_--;
  currently_processing_message_ = NULL;
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

void GpuChannel::StubSchedulingChanged(bool scheduled) {
  bool a_stub_was_descheduled = num_stubs_descheduled_ > 0;
  if (scheduled) {
    num_stubs_descheduled_--;
    OnScheduled();
  } else {
    num_stubs_descheduled_++;
  }
  DCHECK_LE(num_stubs_descheduled_, stubs_.size());
  bool a_stub_is_descheduled = num_stubs_descheduled_ > 0;

  if (a_stub_is_descheduled != a_stub_was_descheduled) {
    if (preempting_flag_.get()) {
      io_message_loop_->PostTask(
          FROM_HERE,
          base::Bind(&GpuChannelMessageFilter::UpdateStubSchedulingState,
                     filter_, a_stub_is_descheduled));
    }
  }
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
      image_manager_,
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
  if (preempted_flag_.get())
    stub->SetPreemptByFlag(preempted_flag_);
  router_.AddRoute(*route_id, stub.get());
  stubs_.AddWithID(stub.release(), *route_id);
#endif  // ENABLE_GPU
}

GpuCommandBufferStub* GpuChannel::LookupCommandBuffer(int32 route_id) {
  return stubs_.Lookup(route_id);
}

void GpuChannel::CreateImage(
    gfx::PluginWindowHandle window,
    int32 image_id,
    gfx::Size* size) {
  TRACE_EVENT1("gpu",
               "GpuChannel::CreateImage",
               "image_id",
               image_id);

  *size = gfx::Size();

  if (image_manager_->LookupImage(image_id)) {
    LOG(ERROR) << "CreateImage failed, image_id already in use.";
    return;
  }

  scoped_refptr<gfx::GLImage> image = gfx::GLImage::CreateGLImage(window);
  if (!image)
    return;

  image_manager_->AddImage(image.get(), image_id);
  *size = image->GetSize();
}

void GpuChannel::DeleteImage(int32 image_id) {
  TRACE_EVENT1("gpu",
               "GpuChannel::DeleteImage",
               "image_id",
               image_id);

  image_manager_->RemoveImage(image_id);
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

gpu::PreemptionFlag* GpuChannel::GetPreemptionFlag() {
  if (!preempting_flag_.get()) {
    preempting_flag_ = new gpu::PreemptionFlag;
    io_message_loop_->PostTask(
        FROM_HERE, base::Bind(
            &GpuChannelMessageFilter::SetPreemptingFlagAndSchedulingState,
            filter_, preempting_flag_, num_stubs_descheduled_ > 0));
  }
  return preempting_flag_.get();
}

void GpuChannel::SetPreemptByFlag(
    scoped_refptr<gpu::PreemptionFlag> preempted_flag) {
  preempted_flag_ = preempted_flag;

  for (StubMap::Iterator<GpuCommandBufferStub> it(&stubs_);
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->SetPreemptByFlag(preempted_flag_);
  }
}

GpuChannel::~GpuChannel() {
  if (preempting_flag_.get())
    preempting_flag_->Reset();
}

void GpuChannel::OnDestroy() {
  TRACE_EVENT0("gpu", "GpuChannel::OnDestroy");
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannel, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateOffscreenCommandBuffer,
                                    OnCreateOffscreenCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroyCommandBuffer,
                                    OnDestroyCommandBuffer)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_RegisterStreamTextureProxy,
                        OnRegisterStreamTextureProxy)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_EstablishStreamTexture,
                        OnEstablishStreamTexture)
#endif
    IPC_MESSAGE_HANDLER(
        GpuChannelMsg_CollectRenderingStatsForSurface,
        OnCollectRenderingStatsForSurface)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << msg.type();
  return handled;
}

void GpuChannel::HandleMessage() {
  handle_messages_scheduled_ = false;
  if (deferred_messages_.empty())
    return;

  bool should_fast_track_ack = false;
  IPC::Message* m = deferred_messages_.front();
  GpuCommandBufferStub* stub = stubs_.Lookup(m->routing_id());

  do {
    if (stub) {
      if (!stub->IsScheduled())
        return;
      if (stub->IsPreempted()) {
        OnScheduled();
        return;
      }
    }

    scoped_ptr<IPC::Message> message(m);
    deferred_messages_.pop_front();
    bool message_processed = true;

    processed_get_state_fast_ =
        (message->type() == GpuCommandBufferMsg_GetStateFast::ID);

    currently_processing_message_ = message.get();
    bool result;
    if (message->routing_id() == MSG_ROUTING_CONTROL)
      result = OnControlMessageReceived(*message);
    else
      result = router_.RouteMessage(*message);
    currently_processing_message_ = NULL;

    if (!result) {
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
          message_processed = false;
        }
      }
    }
    if (message_processed)
      MessageProcessed();

    // We want the EchoACK following the SwapBuffers to be sent as close as
    // possible, avoiding scheduling other channels in the meantime.
    should_fast_track_ack = false;
    if (!deferred_messages_.empty()) {
      m = deferred_messages_.front();
      stub = stubs_.Lookup(m->routing_id());
      should_fast_track_ack =
          (m->type() == GpuCommandBufferMsg_Echo::ID) &&
          stub && stub->IsScheduled();
    }
  } while (should_fast_track_ack);

  if (!deferred_messages_.empty()) {
    OnScheduled();
  }
}

void GpuChannel::OnCreateOffscreenCommandBuffer(
    const gfx::Size& size,
    const GPUCreateCommandBufferConfig& init_params,
    int32* route_id) {
  TRACE_EVENT0("gpu", "GpuChannel::OnCreateOffscreenCommandBuffer");
  GpuCommandBufferStub* share_group = stubs_.Lookup(init_params.share_group_id);

  *route_id = GenerateRouteID();

  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this,
      share_group,
      gfx::GLSurfaceHandle(),
      mailbox_manager_.get(),
      image_manager_.get(),
      size,
      disallowed_features_,
      init_params.allowed_extensions,
      init_params.attribs,
      init_params.gpu_preference,
      *route_id,
      0, watchdog_,
      software_,
      init_params.active_url));
  if (preempted_flag_.get())
    stub->SetPreemptByFlag(preempted_flag_);
  router_.AddRoute(*route_id, stub.get());
  stubs_.AddWithID(stub.release(), *route_id);
  TRACE_EVENT1("gpu", "GpuChannel::OnCreateOffscreenCommandBuffer",
               "route_id", route_id);
}

void GpuChannel::OnDestroyCommandBuffer(int32 route_id) {
  TRACE_EVENT1("gpu", "GpuChannel::OnDestroyCommandBuffer",
               "route_id", route_id);

  if (router_.ResolveRoute(route_id)) {
    GpuCommandBufferStub* stub = stubs_.Lookup(route_id);
    bool need_reschedule = (stub && !stub->IsScheduled());
    router_.RemoveRoute(route_id);
    stubs_.Remove(route_id);
    // In case the renderer is currently blocked waiting for a sync reply from
    // the stub, we need to make sure to reschedule the GpuChannel here.
    if (need_reschedule) {
      // This stub won't get a chance to reschedule, so update the count
      // now.
      StubSchedulingChanged(true);
    }
  }
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
    int32 stream_id, int32 primary_id, int32 secondary_id) {
  stream_texture_manager_->EstablishStreamTexture(
      stream_id, primary_id, secondary_id);
}
#endif

void GpuChannel::OnCollectRenderingStatsForSurface(
    int32 surface_id, GpuRenderingStats* stats) {
  for (StubMap::Iterator<GpuCommandBufferStub> it(&stubs_);
       !it.IsAtEnd(); it.Advance()) {
    int texture_upload_count =
        it.GetCurrentValue()->decoder()->GetTextureUploadCount();
    base::TimeDelta total_texture_upload_time =
        it.GetCurrentValue()->decoder()->GetTotalTextureUploadTime();
    base::TimeDelta total_processing_commands_time =
        it.GetCurrentValue()->decoder()->GetTotalProcessingCommandsTime();

    stats->global_texture_upload_count += texture_upload_count;
    stats->global_total_texture_upload_time += total_texture_upload_time;
    stats->global_total_processing_commands_time +=
        total_processing_commands_time;
    if (it.GetCurrentValue()->surface_id() == surface_id) {
      stats->texture_upload_count += texture_upload_count;
      stats->total_texture_upload_time += total_texture_upload_time;
      stats->total_processing_commands_time += total_processing_commands_time;
    }
  }
}

void GpuChannel::MessageProcessed() {
  messages_processed_++;
  if (preempting_flag_.get()) {
    io_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&GpuChannelMessageFilter::MessageProcessed,
                   filter_, messages_processed_));
  }
}

void GpuChannel::CacheShader(const std::string& key,
                             const std::string& shader) {
  gpu_channel_manager_->Send(
      new GpuHostMsg_CacheShader(client_id_, key, shader));
}

}  // namespace content
