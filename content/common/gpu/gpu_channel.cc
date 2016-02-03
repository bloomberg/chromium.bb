// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_channel.h"

#include <utility>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <algorithm>
#include <deque>
#include <set>
#include <vector>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/media/gpu_jpeg_decode_accelerator.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "ipc/ipc_channel.h"
#include "ipc/message_filter.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

namespace content {
namespace {

// Number of milliseconds between successive vsync. Many GL commands block
// on vsync, so thresholds for preemption should be multiples of this.
const int64_t kVsyncIntervalMs = 17;

// Amount of time that we will wait for an IPC to be processed before
// preempting. After a preemption, we must wait this long before triggering
// another preemption.
const int64_t kPreemptWaitTimeMs = 2 * kVsyncIntervalMs;

// Once we trigger a preemption, the maximum duration that we will wait
// before clearing the preemption.
const int64_t kMaxPreemptTimeMs = kVsyncIntervalMs;

// Stop the preemption once the time for the longest pending IPC drops
// below this threshold.
const int64_t kStopPreemptThresholdMs = kVsyncIntervalMs;

}  // anonymous namespace

scoped_refptr<GpuChannelMessageQueue> GpuChannelMessageQueue::Create(
    const base::WeakPtr<GpuChannel>& gpu_channel,
    base::SingleThreadTaskRunner* task_runner,
    gpu::SyncPointManager* sync_point_manager) {
  return new GpuChannelMessageQueue(gpu_channel, task_runner,
                                    sync_point_manager);
}

scoped_refptr<gpu::SyncPointOrderData>
GpuChannelMessageQueue::GetSyncPointOrderData() {
  return sync_point_order_data_;
}

GpuChannelMessageQueue::GpuChannelMessageQueue(
    const base::WeakPtr<GpuChannel>& gpu_channel,
    base::SingleThreadTaskRunner* task_runner,
    gpu::SyncPointManager* sync_point_manager)
    : enabled_(true),
      sync_point_order_data_(gpu::SyncPointOrderData::Create()),
      gpu_channel_(gpu_channel),
      task_runner_(task_runner),
      sync_point_manager_(sync_point_manager) {}

GpuChannelMessageQueue::~GpuChannelMessageQueue() {
  DCHECK(channel_messages_.empty());
}

uint32_t GpuChannelMessageQueue::GetUnprocessedOrderNum() const {
  return sync_point_order_data_->unprocessed_order_num();
}

uint32_t GpuChannelMessageQueue::GetProcessedOrderNum() const {
  return sync_point_order_data_->processed_order_num();
}

void GpuChannelMessageQueue::PushBackMessage(const IPC::Message& message) {
  base::AutoLock auto_lock(channel_messages_lock_);
  if (enabled_)
    PushMessageHelper(make_scoped_ptr(new GpuChannelMessage(message)));
}

bool GpuChannelMessageQueue::HasQueuedMessages() const {
  base::AutoLock auto_lock(channel_messages_lock_);
  return !channel_messages_.empty();
}

base::TimeTicks GpuChannelMessageQueue::GetNextMessageTimeTick() const {
  base::AutoLock auto_lock(channel_messages_lock_);
  if (!channel_messages_.empty())
    return channel_messages_.front()->time_received;
  return base::TimeTicks();
}

GpuChannelMessage* GpuChannelMessageQueue::GetNextMessage() const {
  base::AutoLock auto_lock(channel_messages_lock_);
  if (!channel_messages_.empty()) {
    DCHECK_GT(channel_messages_.front()->order_number,
              sync_point_order_data_->processed_order_num());
    DCHECK_LE(channel_messages_.front()->order_number,
              sync_point_order_data_->unprocessed_order_num());

    return channel_messages_.front();
  }
  return nullptr;
}

void GpuChannelMessageQueue::BeginMessageProcessing(
    const GpuChannelMessage* msg) {
  sync_point_order_data_->BeginProcessingOrderNumber(msg->order_number);
}

void GpuChannelMessageQueue::PauseMessageProcessing(
    const GpuChannelMessage* msg) {
  sync_point_order_data_->PauseProcessingOrderNumber(msg->order_number);
}

bool GpuChannelMessageQueue::MessageProcessed() {
  base::AutoLock auto_lock(channel_messages_lock_);
  DCHECK(!channel_messages_.empty());
  scoped_ptr<GpuChannelMessage> msg(channel_messages_.front());
  channel_messages_.pop_front();
  sync_point_order_data_->FinishProcessingOrderNumber(msg->order_number);
  return !channel_messages_.empty();
}

void GpuChannelMessageQueue::DeleteAndDisableMessages() {
  {
    base::AutoLock auto_lock(channel_messages_lock_);
    DCHECK(enabled_);
    enabled_ = false;
  }

  // We guarantee that the queues will no longer be modified after enabled_
  // is set to false, it is now safe to modify the queue without the lock.
  // All public facing modifying functions check enabled_ while all
  // private modifying functions DCHECK(enabled_) to enforce this.
  while (!channel_messages_.empty()) {
    scoped_ptr<GpuChannelMessage> msg(channel_messages_.front());
    channel_messages_.pop_front();
  }

  if (sync_point_order_data_) {
    sync_point_order_data_->Destroy();
    sync_point_order_data_ = nullptr;
  }
}

void GpuChannelMessageQueue::ScheduleHandleMessage() {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&GpuChannel::HandleMessage, gpu_channel_));
}

void GpuChannelMessageQueue::PushMessageHelper(
    scoped_ptr<GpuChannelMessage> msg) {
  channel_messages_lock_.AssertAcquired();
  DCHECK(enabled_);

  msg->order_number = sync_point_order_data_->GenerateUnprocessedOrderNumber(
      sync_point_manager_);
  msg->time_received = base::TimeTicks::Now();

  bool had_messages = !channel_messages_.empty();
  channel_messages_.push_back(msg.release());
  if (!had_messages)
    ScheduleHandleMessage();
}

GpuChannelMessageFilter::GpuChannelMessageFilter(
    const base::WeakPtr<GpuChannel>& gpu_channel,
    GpuChannelMessageQueue* message_queue,
    base::SingleThreadTaskRunner* task_runner,
    gpu::PreemptionFlag* preempting_flag)
    : preemption_state_(IDLE),
      gpu_channel_(gpu_channel),
      message_queue_(message_queue),
      sender_(nullptr),
      peer_pid_(base::kNullProcessId),
      task_runner_(task_runner),
      preempting_flag_(preempting_flag),
      a_stub_is_descheduled_(false) {}

GpuChannelMessageFilter::~GpuChannelMessageFilter() {}

void GpuChannelMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(!sender_);
  sender_ = sender;
  timer_ = make_scoped_ptr(new base::OneShotTimer);
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnFilterAdded(sender_);
  }
}

void GpuChannelMessageFilter::OnFilterRemoved() {
  DCHECK(sender_);
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnFilterRemoved();
  }
  sender_ = nullptr;
  peer_pid_ = base::kNullProcessId;
  timer_ = nullptr;
}

void GpuChannelMessageFilter::OnChannelConnected(int32_t peer_pid) {
  DCHECK(peer_pid_ == base::kNullProcessId);
  peer_pid_ = peer_pid;
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnChannelConnected(peer_pid);
  }
}

void GpuChannelMessageFilter::OnChannelError() {
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnChannelError();
  }
}

void GpuChannelMessageFilter::OnChannelClosing() {
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnChannelClosing();
  }
}

void GpuChannelMessageFilter::AddChannelFilter(
    scoped_refptr<IPC::MessageFilter> filter) {
  channel_filters_.push_back(filter);
  if (sender_)
    filter->OnFilterAdded(sender_);
  if (peer_pid_ != base::kNullProcessId)
    filter->OnChannelConnected(peer_pid_);
}

void GpuChannelMessageFilter::RemoveChannelFilter(
    scoped_refptr<IPC::MessageFilter> filter) {
  if (sender_)
    filter->OnFilterRemoved();
  channel_filters_.erase(
      std::find(channel_filters_.begin(), channel_filters_.end(), filter));
}

bool GpuChannelMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(sender_);

  if (message.should_unblock() || message.is_reply()) {
    DLOG(ERROR) << "Unexpected message type";
    return true;
  }

  if (message.type() == GpuChannelMsg_Nop::ID) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    Send(reply);
    return true;
  }

  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    if (filter->OnMessageReceived(message)) {
      return true;
    }
  }

  // Forward all other messages to the GPU Channel.
  if (message.type() == GpuCommandBufferMsg_WaitForTokenInRange::ID ||
      message.type() == GpuCommandBufferMsg_WaitForGetOffsetInRange::ID) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&GpuChannel::HandleOutOfOrderMessage,
                                      gpu_channel_, message));
  } else {
    message_queue_->PushBackMessage(message);
  }

  UpdatePreemptionState();
  return true;
}

void GpuChannelMessageFilter::OnMessageProcessed() {
  UpdatePreemptionState();
}

void GpuChannelMessageFilter::UpdateStubSchedulingState(
    bool a_stub_is_descheduled) {
  a_stub_is_descheduled_ = a_stub_is_descheduled;
  UpdatePreemptionState();
}

bool GpuChannelMessageFilter::Send(IPC::Message* message) {
  return sender_->Send(message);
}

void GpuChannelMessageFilter::UpdatePreemptionState() {
  switch (preemption_state_) {
    case IDLE:
      if (preempting_flag_.get() && message_queue_->HasQueuedMessages())
        TransitionToWaiting();
      break;
    case WAITING:
      // A timer will transition us to CHECKING.
      DCHECK(timer_->IsRunning());
      break;
    case CHECKING: {
      base::TimeTicks time_tick = message_queue_->GetNextMessageTimeTick();
      if (!time_tick.is_null()) {
        base::TimeDelta time_elapsed = base::TimeTicks::Now() - time_tick;
        if (time_elapsed.InMilliseconds() < kPreemptWaitTimeMs) {
          // Schedule another check for when the IPC may go long.
          timer_->Start(FROM_HERE,
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
    } break;
    case PREEMPTING:
      // A TransitionToIdle() timer should always be running in this state.
      DCHECK(timer_->IsRunning());
      if (a_stub_is_descheduled_)
        TransitionToWouldPreemptDescheduled();
      else
        TransitionToIdleIfCaughtUp();
      break;
    case WOULD_PREEMPT_DESCHEDULED:
      // A TransitionToIdle() timer should never be running in this state.
      DCHECK(!timer_->IsRunning());
      if (!a_stub_is_descheduled_)
        TransitionToPreempting();
      else
        TransitionToIdleIfCaughtUp();
      break;
    default:
      NOTREACHED();
  }
}

void GpuChannelMessageFilter::TransitionToIdleIfCaughtUp() {
  DCHECK(preemption_state_ == PREEMPTING ||
         preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
  base::TimeTicks next_tick = message_queue_->GetNextMessageTimeTick();
  if (next_tick.is_null()) {
    TransitionToIdle();
  } else {
    base::TimeDelta time_elapsed = base::TimeTicks::Now() - next_tick;
    if (time_elapsed.InMilliseconds() < kStopPreemptThresholdMs)
      TransitionToIdle();
  }
}

void GpuChannelMessageFilter::TransitionToIdle() {
  DCHECK(preemption_state_ == PREEMPTING ||
         preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
  // Stop any outstanding timer set to force us from PREEMPTING to IDLE.
  timer_->Stop();

  preemption_state_ = IDLE;
  preempting_flag_->Reset();
  TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 0);

  UpdatePreemptionState();
}

void GpuChannelMessageFilter::TransitionToWaiting() {
  DCHECK_EQ(preemption_state_, IDLE);
  DCHECK(!timer_->IsRunning());

  preemption_state_ = WAITING;
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromMilliseconds(kPreemptWaitTimeMs), this,
                &GpuChannelMessageFilter::TransitionToChecking);
}

void GpuChannelMessageFilter::TransitionToChecking() {
  DCHECK_EQ(preemption_state_, WAITING);
  DCHECK(!timer_->IsRunning());

  preemption_state_ = CHECKING;
  max_preemption_time_ = base::TimeDelta::FromMilliseconds(kMaxPreemptTimeMs);
  UpdatePreemptionState();
}

void GpuChannelMessageFilter::TransitionToPreempting() {
  DCHECK(preemption_state_ == CHECKING ||
         preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
  DCHECK(!a_stub_is_descheduled_);

  // Stop any pending state update checks that we may have queued
  // while CHECKING.
  if (preemption_state_ == CHECKING)
    timer_->Stop();

  preemption_state_ = PREEMPTING;
  preempting_flag_->Set();
  TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 1);

  timer_->Start(FROM_HERE, max_preemption_time_, this,
                &GpuChannelMessageFilter::TransitionToIdle);

  UpdatePreemptionState();
}

void GpuChannelMessageFilter::TransitionToWouldPreemptDescheduled() {
  DCHECK(preemption_state_ == CHECKING || preemption_state_ == PREEMPTING);
  DCHECK(a_stub_is_descheduled_);

  if (preemption_state_ == CHECKING) {
    // Stop any pending state update checks that we may have queued
    // while CHECKING.
    timer_->Stop();
  } else {
    // Stop any TransitionToIdle() timers that we may have queued
    // while PREEMPTING.
    timer_->Stop();
    max_preemption_time_ = timer_->desired_run_time() - base::TimeTicks::Now();
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

GpuChannel::StreamState::StreamState(int32_t id, GpuStreamPriority priority)
    : id_(id), priority_(priority) {}

GpuChannel::StreamState::~StreamState() {}

void GpuChannel::StreamState::AddRoute(int32_t route_id) {
  routes_.insert(route_id);
}
void GpuChannel::StreamState::RemoveRoute(int32_t route_id) {
  routes_.erase(route_id);
}

bool GpuChannel::StreamState::HasRoute(int32_t route_id) const {
  return routes_.find(route_id) != routes_.end();
}

bool GpuChannel::StreamState::HasRoutes() const {
  return !routes_.empty();
}

GpuChannel::GpuChannel(GpuChannelManager* gpu_channel_manager,
                       gpu::SyncPointManager* sync_point_manager,
                       GpuWatchdog* watchdog,
                       gfx::GLShareGroup* share_group,
                       gpu::gles2::MailboxManager* mailbox,
                       gpu::PreemptionFlag* preempting_flag,
                       base::SingleThreadTaskRunner* task_runner,
                       base::SingleThreadTaskRunner* io_task_runner,
                       int client_id,
                       uint64_t client_tracing_id,
                       bool allow_view_command_buffers,
                       bool allow_real_time_streams)
    : gpu_channel_manager_(gpu_channel_manager),
      sync_point_manager_(sync_point_manager),
      channel_id_(IPC::Channel::GenerateVerifiedChannelID("gpu")),
      preempting_flag_(preempting_flag),
      client_id_(client_id),
      client_tracing_id_(client_tracing_id),
      task_runner_(task_runner),
      io_task_runner_(io_task_runner),
      share_group_(share_group),
      mailbox_manager_(mailbox),
      subscription_ref_set_(new gpu::gles2::SubscriptionRefSet),
      pending_valuebuffer_state_(new gpu::ValueStateMap),
      watchdog_(watchdog),
      num_stubs_descheduled_(0),
      allow_view_command_buffers_(allow_view_command_buffers),
      allow_real_time_streams_(allow_real_time_streams),
      weak_factory_(this) {
  DCHECK(gpu_channel_manager);
  DCHECK(client_id);

  message_queue_ = GpuChannelMessageQueue::Create(
      weak_factory_.GetWeakPtr(), task_runner, sync_point_manager);

  filter_ = new GpuChannelMessageFilter(
      weak_factory_.GetWeakPtr(), message_queue_.get(), task_runner,
      preempting_flag);

  subscription_ref_set_->AddObserver(this);
}

GpuChannel::~GpuChannel() {
  // Clear stubs first because of dependencies.
  stubs_.clear();

  message_queue_->DeleteAndDisableMessages();

  subscription_ref_set_->RemoveObserver(this);
  if (preempting_flag_.get())
    preempting_flag_->Reset();
}

IPC::ChannelHandle GpuChannel::Init(base::WaitableEvent* shutdown_event) {
  DCHECK(shutdown_event);
  DCHECK(!channel_);

  IPC::ChannelHandle channel_handle(channel_id_);

  channel_ =
      IPC::SyncChannel::Create(channel_handle, IPC::Channel::MODE_SERVER, this,
                               io_task_runner_, false, shutdown_event);

#if defined(OS_POSIX)
  // On POSIX, pass the renderer-side FD. Also mark it as auto-close so
  // that it gets closed after it has been sent.
  base::ScopedFD renderer_fd = channel_->TakeClientFileDescriptor();
  DCHECK(renderer_fd.is_valid());
  channel_handle.socket = base::FileDescriptor(std::move(renderer_fd));
#endif

  channel_->AddFilter(filter_.get());

  return channel_handle;
}

base::ProcessId GpuChannel::GetClientPID() const {
  return channel_->GetPeerPID();
}

uint32_t GpuChannel::GetProcessedOrderNum() const {
  return message_queue_->GetProcessedOrderNum();
}

uint32_t GpuChannel::GetUnprocessedOrderNum() const {
  return message_queue_->GetUnprocessedOrderNum();
}

bool GpuChannel::OnMessageReceived(const IPC::Message& message) {
  // All messages should be pushed to channel_messages_ and handled separately.
  NOTREACHED();
  return false;
}

void GpuChannel::OnChannelError() {
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::Send(IPC::Message* message) {
  // The GPU process must never send a synchronous IPC message to the renderer
  // process. This could result in deadlock.
  DCHECK(!message->is_sync());

  DVLOG(1) << "sending message @" << message << " on channel @" << this
           << " with type " << message->type();

  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void GpuChannel::OnAddSubscription(unsigned int target) {
  gpu_channel_manager()->Send(
      new GpuHostMsg_AddSubscription(client_id_, target));
}

void GpuChannel::OnRemoveSubscription(unsigned int target) {
  gpu_channel_manager()->Send(
      new GpuHostMsg_RemoveSubscription(client_id_, target));
}

void GpuChannel::OnStubSchedulingChanged(GpuCommandBufferStub* stub,
                                         bool scheduled) {
  bool a_stub_was_descheduled = num_stubs_descheduled_ > 0;
  if (scheduled) {
    num_stubs_descheduled_--;
    ScheduleHandleMessage();
  } else {
    num_stubs_descheduled_++;
  }
  DCHECK_LE(num_stubs_descheduled_, stubs_.size());
  bool a_stub_is_descheduled = num_stubs_descheduled_ > 0;

  if (a_stub_is_descheduled != a_stub_was_descheduled) {
    if (preempting_flag_.get()) {
      io_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&GpuChannelMessageFilter::UpdateStubSchedulingState,
                     filter_, a_stub_is_descheduled));
    }
  }
}

GpuCommandBufferStub* GpuChannel::LookupCommandBuffer(int32_t route_id) {
  return stubs_.get(route_id);
}

void GpuChannel::LoseAllContexts() {
  gpu_channel_manager_->LoseAllContexts();
}

void GpuChannel::MarkAllContextsLost() {
  for (auto& kv : stubs_)
    kv.second->MarkContextLost();
}

bool GpuChannel::AddRoute(int32_t route_id, IPC::Listener* listener) {
  return router_.AddRoute(route_id, listener);
}

void GpuChannel::RemoveRoute(int32_t route_id) {
  router_.RemoveRoute(route_id);
}

void GpuChannel::SetPreemptByFlag(
    scoped_refptr<gpu::PreemptionFlag> preempted_flag) {
  DCHECK(stubs_.empty());
  preempted_flag_ = preempted_flag;
}

void GpuChannel::OnDestroy() {
  TRACE_EVENT0("gpu", "GpuChannel::OnDestroy");
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannel, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateViewCommandBuffer,
                        OnCreateViewCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateOffscreenCommandBuffer,
                        OnCreateOffscreenCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroyCommandBuffer,
                        OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuChannelMsg_CreateJpegDecoder,
                                    OnCreateJpegDecoder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << msg.type();
  return handled;
}

scoped_refptr<gpu::SyncPointOrderData> GpuChannel::GetSyncPointOrderData() {
  return message_queue_->GetSyncPointOrderData();
}

void GpuChannel::HandleMessage() {
  // If we have been preempted by another channel, just post a task to wake up.
  if (preempted_flag_ && preempted_flag_->IsSet()) {
    ScheduleHandleMessage();
    return;
  }

  GpuChannelMessage* m = message_queue_->GetNextMessage();

  // TODO(sunnyps): This could be a DCHECK maybe?
  if (!m)
    return;

  const IPC::Message& message = m->message;
  message_queue_->BeginMessageProcessing(m);
  int32_t routing_id = message.routing_id();
  GpuCommandBufferStub* stub = stubs_.get(routing_id);

  DCHECK(!stub || stub->IsScheduled());

  DVLOG(1) << "received message @" << &message << " on channel @" << this
           << " with type " << message.type();

  HandleMessageHelper(message);

  // A command buffer may be descheduled or preempted but only in the middle of
  // a flush. In this case we should not pop the message from the queue.
  if (stub && stub->HasUnprocessedCommands()) {
    DCHECK_EQ((uint32_t)GpuCommandBufferMsg_AsyncFlush::ID, message.type());
    message_queue_->PauseMessageProcessing(m);
    // If the stub is still scheduled then we were preempted and need to
    // schedule a wakeup otherwise some other event will wake us up e.g. sync
    // point completion. No DCHECK for preemption flag because that can change
    // any time.
    if (stub->IsScheduled())
      ScheduleHandleMessage();
    return;
  }

  if (message_queue_->MessageProcessed())
    ScheduleHandleMessage();

  if (preempting_flag_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&GpuChannelMessageFilter::OnMessageProcessed, filter_));
  }
}

void GpuChannel::ScheduleHandleMessage() {
  task_runner_->PostTask(FROM_HERE, base::Bind(&GpuChannel::HandleMessage,
                                               weak_factory_.GetWeakPtr()));
}

void GpuChannel::HandleMessageHelper(const IPC::Message& msg) {
  int32_t routing_id = msg.routing_id();

  bool handled = false;
  if (routing_id == MSG_ROUTING_CONTROL) {
    handled = OnControlMessageReceived(msg);
  } else {
    handled = router_.RouteMessage(msg);
  }

  // Respond to sync messages even if router failed to route.
  if (!handled && msg.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
    reply->set_reply_error();
    Send(reply);
  }
}

void GpuChannel::HandleOutOfOrderMessage(const IPC::Message& msg) {
  switch (msg.type()) {
    case GpuCommandBufferMsg_WaitForGetOffsetInRange::ID:
    case GpuCommandBufferMsg_WaitForTokenInRange::ID:
      router_.RouteMessage(msg);
      break;
    default:
      NOTREACHED();
  }
}

void GpuChannel::HandleMessageForTesting(const IPC::Message& msg) {
  HandleMessageHelper(msg);
}

#if defined(OS_ANDROID)
const GpuCommandBufferStub* GpuChannel::GetOneStub() const {
  for (const auto& kv : stubs_) {
    const GpuCommandBufferStub* stub = kv.second;
    if (stub->decoder() && !stub->decoder()->WasContextLost())
      return stub;
  }
  return nullptr;
}
#endif

void GpuChannel::OnCreateViewCommandBuffer(
    const gfx::GLSurfaceHandle& window,
    const GPUCreateCommandBufferConfig& init_params,
    int32_t route_id,
    bool* succeeded) {
  TRACE_EVENT1("gpu", "GpuChannel::CreateViewCommandBuffer", "route_id",
               route_id);
  *succeeded = false;
  if (allow_view_command_buffers_ && !window.is_null()) {
    *succeeded =
        CreateCommandBuffer(window, gfx::Size(), init_params, route_id);
  }
}

void GpuChannel::OnCreateOffscreenCommandBuffer(
    const gfx::Size& size,
    const GPUCreateCommandBufferConfig& init_params,
    int32_t route_id,
    bool* succeeded) {
  TRACE_EVENT1("gpu", "GpuChannel::OnCreateOffscreenCommandBuffer", "route_id",
               route_id);
  *succeeded =
      CreateCommandBuffer(gfx::GLSurfaceHandle(), size, init_params, route_id);
}

bool GpuChannel::CreateCommandBuffer(
    const gfx::GLSurfaceHandle& window,
    const gfx::Size& size,
    const GPUCreateCommandBufferConfig& init_params,
    int32_t route_id) {
  int32_t share_group_id = init_params.share_group_id;
  GpuCommandBufferStub* share_group = stubs_.get(share_group_id);

  if (!share_group && share_group_id != MSG_ROUTING_NONE)
    return false;

  int32_t stream_id = init_params.stream_id;
  GpuStreamPriority stream_priority = init_params.stream_priority;

  if (share_group && stream_id != share_group->stream_id())
    return false;

  if (!allow_real_time_streams_ &&
      stream_priority == GpuStreamPriority::REAL_TIME) {
    return false;
  }

  auto stream_it = streams_.find(stream_id);
  if (stream_it != streams_.end() &&
      stream_priority != GpuStreamPriority::INHERIT &&
      stream_priority != stream_it->second.priority()) {
    return false;
  }

  bool offscreen = window.is_null();
  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this, sync_point_manager_, task_runner_.get(), share_group, window,
      mailbox_manager_.get(), preempted_flag_.get(),
      subscription_ref_set_.get(), pending_valuebuffer_state_.get(), size,
      disallowed_features_, init_params.attribs, init_params.gpu_preference,
      init_params.stream_id, route_id, offscreen, watchdog_,
      init_params.active_url));

  if (!router_.AddRoute(route_id, stub.get())) {
    DLOG(ERROR) << "GpuChannel::CreateCommandBuffer(): failed to add route";
    return false;
  }

  if (stream_it != streams_.end()) {
    stream_it->second.AddRoute(route_id);
  } else {
    StreamState stream(stream_id, stream_priority);
    stream.AddRoute(route_id);
    streams_.insert(std::make_pair(stream_id, stream));
  }

  stubs_.set(route_id, std::move(stub));
  return true;
}

void GpuChannel::OnDestroyCommandBuffer(int32_t route_id) {
  TRACE_EVENT1("gpu", "GpuChannel::OnDestroyCommandBuffer",
               "route_id", route_id);

  scoped_ptr<GpuCommandBufferStub> stub = stubs_.take_and_erase(route_id);

  if (!stub)
    return;

  router_.RemoveRoute(route_id);

  int32_t stream_id = stub->stream_id();
  auto stream_it = streams_.find(stream_id);
  DCHECK(stream_it != streams_.end());
  stream_it->second.RemoveRoute(route_id);
  if (!stream_it->second.HasRoutes())
    streams_.erase(stream_it);

  // In case the renderer is currently blocked waiting for a sync reply from the
  // stub, we need to make sure to reschedule the GpuChannel here.
  if (!stub->IsScheduled()) {
    // This stub won't get a chance to reschedule, so update the count now.
    OnStubSchedulingChanged(stub.get(), true);
  }
}

void GpuChannel::OnCreateJpegDecoder(int32_t route_id,
                                     IPC::Message* reply_msg) {
  if (!jpeg_decoder_) {
    jpeg_decoder_.reset(new GpuJpegDecodeAccelerator(this, io_task_runner_));
  }
  jpeg_decoder_->AddClient(route_id, reply_msg);
}

void GpuChannel::CacheShader(const std::string& key,
                             const std::string& shader) {
  gpu_channel_manager_->Send(
      new GpuHostMsg_CacheShader(client_id_, key, shader));
}

void GpuChannel::AddFilter(IPC::MessageFilter* filter) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuChannelMessageFilter::AddChannelFilter,
                            filter_, make_scoped_refptr(filter)));
}

void GpuChannel::RemoveFilter(IPC::MessageFilter* filter) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuChannelMessageFilter::RemoveChannelFilter,
                            filter_, make_scoped_refptr(filter)));
}

uint64_t GpuChannel::GetMemoryUsage() {
  // Collect the unique memory trackers in use by the |stubs_|.
  std::set<gpu::gles2::MemoryTracker*> unique_memory_trackers;
  for (auto& kv : stubs_)
    unique_memory_trackers.insert(kv.second->GetMemoryTracker());

  // Sum the memory usage for all unique memory trackers.
  uint64_t size = 0;
  for (auto* tracker : unique_memory_trackers) {
    size += gpu_channel_manager()->gpu_memory_manager()->GetTrackerMemoryUsage(
        tracker);
  }

  return size;
}

scoped_refptr<gl::GLImage> GpuChannel::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint32_t internalformat) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      if (!base::IsValueInRangeForNumericType<size_t>(handle.stride))
        return nullptr;
      scoped_refptr<gl::GLImageSharedMemory> image(
          new gl::GLImageSharedMemory(size, internalformat));
      if (!image->Initialize(handle.handle, handle.id, format, handle.offset,
                             handle.stride)) {
        return nullptr;
      }

      return image;
    }
    default: {
      GpuChannelManager* manager = gpu_channel_manager();
      if (!manager->gpu_memory_buffer_factory())
        return nullptr;

      return manager->gpu_memory_buffer_factory()
          ->AsImageFactory()
          ->CreateImageForGpuMemoryBuffer(handle,
                                          size,
                                          format,
                                          internalformat,
                                          client_id_);
    }
  }
}

void GpuChannel::HandleUpdateValueState(
    unsigned int target, const gpu::ValueState& state) {
  pending_valuebuffer_state_->UpdateState(target, state);
}

}  // namespace content
