// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/hash.h"
#include "base/shared_memory.h"
#include "base/time.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_watchdog.h"
#include "content/common/gpu/image_transport_surface.h"
#include "content/common/gpu/media/gpu_video_decode_accelerator.h"
#include "content/common/gpu/sync_point_manager.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/stream_texture_manager_android.h"
#endif

namespace content {
namespace {

// The GpuCommandBufferMemoryTracker class provides a bridge between the
// ContextGroup's memory type managers and the GpuMemoryManager class.
class GpuCommandBufferMemoryTracker : public gpu::gles2::MemoryTracker {
 public:
  GpuCommandBufferMemoryTracker(GpuChannel* channel) :
      tracking_group_(channel->gpu_channel_manager()->gpu_memory_manager()->
          CreateTrackingGroup(channel->renderer_pid(), this)) {
  }

  void TrackMemoryAllocatedChange(size_t old_size,
                                  size_t new_size,
                                  gpu::gles2::MemoryTracker::Pool pool) {
    tracking_group_->TrackMemoryAllocatedChange(
        old_size, new_size, pool);
  }

 private:
  ~GpuCommandBufferMemoryTracker() {
  }
  scoped_ptr<GpuMemoryTrackingGroup> tracking_group_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferMemoryTracker);
};

// FastSetActiveURL will shortcut the expensive call to SetActiveURL when the
// url_hash matches.
void FastSetActiveURL(const GURL& url, size_t url_hash) {
  // Leave the previously set URL in the empty case -- empty URLs are given by
  // WebKitPlatformSupportImpl::createOffscreenGraphicsContext3D. Hopefully the
  // onscreen context URL was set previously and will show up even when a crash
  // occurs during offscreen command processing.
  if (url.is_empty())
    return;
  static size_t g_last_url_hash = 0;
  if (url_hash != g_last_url_hash) {
    g_last_url_hash = url_hash;
    GetContentClient()->SetActiveURL(url);
  }
}

// The first time polling a fence, delay some extra time to allow other
// stubs to process some work, or else the timing of the fences could
// allow a pattern of alternating fast and slow frames to occur.
const int64 kHandleMoreWorkPeriodMs = 2;
const int64 kHandleMoreWorkPeriodBusyMs = 1;

}  // namespace

GpuCommandBufferStub::GpuCommandBufferStub(
    GpuChannel* channel,
    GpuCommandBufferStub* share_group,
    const gfx::GLSurfaceHandle& handle,
    gpu::gles2::MailboxManager* mailbox_manager,
    gpu::gles2::ImageManager* image_manager,
    const gfx::Size& size,
    const gpu::gles2::DisallowedFeatures& disallowed_features,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    gfx::GpuPreference gpu_preference,
    int32 route_id,
    int32 surface_id,
    GpuWatchdog* watchdog,
    bool software,
    const GURL& active_url)
    : channel_(channel),
      handle_(handle),
      initial_size_(size),
      disallowed_features_(disallowed_features),
      allowed_extensions_(allowed_extensions),
      requested_attribs_(attribs),
      gpu_preference_(gpu_preference),
      route_id_(route_id),
      surface_id_(surface_id),
      software_(software),
      last_flush_count_(0),
      parent_stub_for_initialization_(),
      parent_texture_for_initialization_(0),
      watchdog_(watchdog),
      sync_point_wait_count_(0),
      delayed_work_scheduled_(false),
      active_url_(active_url),
      total_gpu_memory_(0) {
  active_url_hash_ = base::Hash(active_url.possibly_invalid_spec());
  FastSetActiveURL(active_url_, active_url_hash_);
  if (share_group) {
    context_group_ = share_group->context_group_;
  } else {
    context_group_ = new gpu::gles2::ContextGroup(
        mailbox_manager,
        image_manager,
        new GpuCommandBufferMemoryTracker(channel),
        true);
  }
}

GpuCommandBufferStub::~GpuCommandBufferStub() {
  Destroy();

  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->Send(new GpuHostMsg_DestroyCommandBuffer(surface_id()));
}

GpuMemoryManager* GpuCommandBufferStub::GetMemoryManager() {
    return channel()->gpu_channel_manager()->gpu_memory_manager();
}

bool GpuCommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  FastSetActiveURL(active_url_, active_url_hash_);

  // Ensure the appropriate GL context is current before handling any IPC
  // messages directed at the command buffer. This ensures that the message
  // handler can assume that the context is current (not necessary for
  // Echo, RetireSyncPoint, or WaitSyncPoint).
  if (decoder_.get() &&
      message.type() != GpuCommandBufferMsg_Echo::ID &&
      message.type() != GpuCommandBufferMsg_RetireSyncPoint::ID &&
      message.type() != GpuCommandBufferMsg_WaitSyncPoint::ID) {
    if (!MakeCurrent())
      return false;
  }

  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuCommandBufferStub, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_Initialize,
                                    OnInitialize);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_SetGetBuffer,
                                    OnSetGetBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_SetParent,
                                    OnSetParent);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Echo, OnEcho);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetState, OnGetState);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetStateFast,
                                    OnGetStateFast);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Rescheduled, OnRescheduled);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RegisterTransferBuffer,
                        OnRegisterTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyTransferBuffer,
                        OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetTransferBuffer,
                                    OnGetTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateVideoDecoder,
                                    OnCreateVideoDecoder)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyVideoDecoder,
                        OnDestroyVideoDecoder)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetSurfaceVisible,
                        OnSetSurfaceVisible)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DiscardBackbuffer,
                        OnDiscardBackbuffer)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_EnsureBackbuffer,
                        OnEnsureBackbuffer)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RetireSyncPoint,
                        OnRetireSyncPoint)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_WaitSyncPoint,
                        OnWaitSyncPoint)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalSyncPoint,
                        OnSignalSyncPoint)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SendClientManagedMemoryStats,
                        OnReceivedClientManagedMemoryStats)
    IPC_MESSAGE_HANDLER(
        GpuCommandBufferMsg_SetClientHasMemoryAllocationChangedCallback,
        OnSetClientHasMemoryAllocationChangedCallback)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Ensure that any delayed work that was created will be handled.
  ScheduleDelayedWork(kHandleMoreWorkPeriodMs);

  DCHECK(handled);
  return handled;
}

bool GpuCommandBufferStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}

bool GpuCommandBufferStub::IsScheduled() {
  return sync_point_wait_count_ == 0 &&
      (!scheduler_.get() || scheduler_->IsScheduled());
}

bool GpuCommandBufferStub::HasMoreWork() {
  return scheduler_.get() && scheduler_->HasMoreWork();
}

void GpuCommandBufferStub::PollWork() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::PollWork");
  delayed_work_scheduled_ = false;
  FastSetActiveURL(active_url_, active_url_hash_);
  if (decoder_.get() && !MakeCurrent())
    return;
  if (scheduler_.get())
    scheduler_->PollUnscheduleFences();
  ScheduleDelayedWork(kHandleMoreWorkPeriodBusyMs);
}

bool GpuCommandBufferStub::HasUnprocessedCommands() {
  if (command_buffer_.get()) {
    gpu::CommandBuffer::State state = command_buffer_->GetLastState();
    return state.put_offset != state.get_offset &&
        !gpu::error::IsError(state.error);
  }
  return false;
}

void GpuCommandBufferStub::ScheduleDelayedWork(int64 delay) {
  if (HasMoreWork() && !delayed_work_scheduled_) {
    delayed_work_scheduled_ = true;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GpuCommandBufferStub::PollWork,
                   AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(delay));
  }
}

void GpuCommandBufferStub::OnEcho(const IPC::Message& message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnEcho");
  Send(new IPC::Message(message));
}

void GpuCommandBufferStub::DelayEcho(IPC::Message* message) {
  delayed_echos_.push_back(message);
}

void GpuCommandBufferStub::OnReschedule() {
  if (!IsScheduled())
    return;
  while (!delayed_echos_.empty()) {
    scoped_ptr<IPC::Message> message(delayed_echos_.front());
    delayed_echos_.pop_front();

    OnMessageReceived(*message);
  }

  channel_->OnScheduled();
}

bool GpuCommandBufferStub::MakeCurrent() {
  if (decoder_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
  command_buffer_->SetParseError(gpu::error::kLostContext);
  if (gfx::GLContext::LosesAllContextsOnContextLost())
    channel_->LoseAllContexts();
  return false;
}

void GpuCommandBufferStub::Destroy() {
  if (handle_.is_null() && !active_url_.is_empty()) {
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    gpu_channel_manager->Send(new GpuHostMsg_DidDestroyOffscreenContext(
        active_url_));
  }

  memory_manager_client_state_.reset();

  while (!sync_points_.empty())
    OnRetireSyncPoint(sync_points_.front());

  // The scheduler has raw references to the decoder and the command buffer so
  // destroy it before those.
  scheduler_.reset();

  while (!delayed_echos_.empty()) {
    delete delayed_echos_.front();
    delayed_echos_.pop_front();
  }

  bool have_context = false;
  if (decoder_.get())
    have_context = decoder_->MakeCurrent();
  FOR_EACH_OBSERVER(DestructionObserver,
                    destruction_observers_,
                    OnWillDestroyStub(this));

  scoped_refptr<gfx::GLContext> context;
  if (decoder_.get()) {
    context = decoder_->GetGLContext();
    decoder_->Destroy(have_context);
    decoder_.reset();
  }

  command_buffer_.reset();

  // Make sure that context_ is current while we destroy surface_, because
  // surface_ may have GL resources that it needs to destroy, and will need
  // context_ to be current in order to not leak these resources.
  if (context)
    context->MakeCurrent(surface_.get());
  surface_ = NULL;
  if (context)
    context->ReleaseCurrent(NULL);
}

void GpuCommandBufferStub::OnInitializeFailed(IPC::Message* reply_message) {
  Destroy();
  GpuCommandBufferMsg_Initialize::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void GpuCommandBufferStub::OnInitialize(
    base::SharedMemoryHandle shared_state_handle,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnInitialize");
  DCHECK(!command_buffer_.get());

  scoped_ptr<base::SharedMemory> shared_state_shm(
      new base::SharedMemory(shared_state_handle, false));

  command_buffer_.reset(new gpu::CommandBufferService(
      context_group_->transfer_buffer_manager()));

  if (!command_buffer_->Initialize()) {
    DLOG(ERROR) << "CommandBufferService failed to initialize.\n";
    OnInitializeFailed(reply_message);
    return;
  }

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group_.get()));

  scheduler_.reset(new gpu::GpuScheduler(command_buffer_.get(),
                                         decoder_.get(),
                                         decoder_.get()));
  if (preempt_by_counter_.get())
    scheduler_->SetPreemptByCounter(preempt_by_counter_);

  decoder_->set_engine(scheduler_.get());

  if (!handle_.is_null()) {
#if defined(OS_MACOSX) || defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
    if (software_) {
      DLOG(ERROR) << "No software support.\n";
      OnInitializeFailed(reply_message);
      return;
    }
#endif

    surface_ = ImageTransportSurface::CreateSurface(
        channel_->gpu_channel_manager(),
        this,
        handle_);
  } else {
    GpuChannelManager* manager = channel_->gpu_channel_manager();
    surface_ = manager->GetDefaultOffscreenSurface();
  }

  if (!surface_.get()) {
    // Ensure the decoder is not destroyed if it is not initialized.
    decoder_.reset();

    DLOG(ERROR) << "Failed to create surface.\n";
    OnInitializeFailed(reply_message);
    return;
  }

  scoped_refptr<gfx::GLContext> context;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVirtualGLContexts) && channel_->share_group()) {
    context = channel_->share_group()->GetSharedContext();
    if (!context) {
      context = gfx::GLContext::CreateGLContext(
          channel_->share_group(),
          channel_->gpu_channel_manager()->GetDefaultOffscreenSurface(),
          gpu_preference_);
      channel_->share_group()->SetSharedContext(context);
    }
    // This should be a non-virtual GL context.
    DCHECK(context->GetHandle());
    context = new gpu::GLContextVirtual(channel_->share_group(),
                                        context,
                                        decoder_->AsWeakPtr());
    if (!context->Initialize(surface_, gpu_preference_)) {
      // TODO(sievers): The real context created above for the default
      // offscreen surface might not be compatible with this surface.
      // Need to adjust at least GLX to be able to create the initial context
      // with a config that is compatible with onscreen and offscreen surfaces.
      context = NULL;
      LOG(FATAL) << "Failed to initialize virtual GL context.";
    } else {
      LOG(INFO) << "Created virtual GL context.";
    }
  }
  if (!context) {
    context = gfx::GLContext::CreateGLContext(
        channel_->share_group(),
        surface_.get(),
        gpu_preference_);
  }
  if (!context) {
    // Ensure the decoder is not destroyed if it is not initialized.
    decoder_.reset();

    DLOG(ERROR) << "Failed to create context.\n";
    OnInitializeFailed(reply_message);
    return;
  }

  if (!context->MakeCurrent(surface_.get())) {
    // Ensure the decoder is not destroyed if it is not initialized.
    decoder_.reset();
    LOG(ERROR) << "Failed to make context current.";
    OnInitializeFailed(reply_message);
    return;
  }

  if (!context->GetTotalGpuMemory(&total_gpu_memory_))
    total_gpu_memory_ = 0;

  if (!context_group_->has_program_cache()) {
    context_group_->set_program_cache(
        channel_->gpu_channel_manager()->program_cache());
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  if (!decoder_->Initialize(surface_,
                            context,
                            !surface_id(),
                            initial_size_,
                            disallowed_features_,
                            allowed_extensions_.c_str(),
                            requested_attribs_)) {
    DLOG(ERROR) << "Failed to initialize decoder.";
    OnInitializeFailed(reply_message);
    return;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGPUServiceLogging)) {
    decoder_->set_log_commands(true);
  }

  decoder_->SetMsgCallback(
      base::Bind(&GpuCommandBufferStub::SendConsoleMessage,
                 base::Unretained(this)));

  command_buffer_->SetPutOffsetChangeCallback(
      base::Bind(&GpuCommandBufferStub::PutChanged, base::Unretained(this)));
  command_buffer_->SetGetBufferChangeCallback(
      base::Bind(&gpu::GpuScheduler::SetGetBuffer,
                 base::Unretained(scheduler_.get())));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&GpuCommandBufferStub::OnParseError, base::Unretained(this)));
  scheduler_->SetScheduledCallback(
      base::Bind(&GpuCommandBufferStub::OnReschedule, base::Unretained(this)));

  if (watchdog_) {
    scheduler_->SetCommandProcessedCallback(
        base::Bind(&GpuCommandBufferStub::OnCommandProcessed,
                   base::Unretained(this)));
  }

#if defined(OS_ANDROID)
  decoder_->SetStreamTextureManager(channel_->stream_texture_manager());
#endif

  if (parent_stub_for_initialization_) {
    decoder_->SetParent(parent_stub_for_initialization_->decoder_.get(),
                        parent_texture_for_initialization_);
    parent_stub_for_initialization_.reset();
    parent_texture_for_initialization_ = 0;
  }

  if (!command_buffer_->SetSharedStateBuffer(shared_state_shm.Pass())) {
    DLOG(ERROR) << "Failed to map shared stae buffer.";
    OnInitializeFailed(reply_message);
    return;
  }

  GpuCommandBufferMsg_Initialize::WriteReplyParams(reply_message, true);
  Send(reply_message);

  if (handle_.is_null() && !active_url_.is_empty()) {
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    gpu_channel_manager->Send(new GpuHostMsg_DidCreateOffscreenContext(
        active_url_));
  }
}

void GpuCommandBufferStub::OnSetGetBuffer(int32 shm_id,
                                          IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnSetGetBuffer");
  if (command_buffer_.get())
    command_buffer_->SetGetBuffer(shm_id);
  Send(reply_message);
}

void GpuCommandBufferStub::OnSetParent(int32 parent_route_id,
                                       uint32 parent_texture_id,
                                       IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnSetParent");
  GpuCommandBufferStub* parent_stub = NULL;
  if (parent_route_id != MSG_ROUTING_NONE) {
    parent_stub = channel_->LookupCommandBuffer(parent_route_id);
  }

  bool result = true;
  if (scheduler_.get()) {
    gpu::gles2::GLES2Decoder* parent_decoder =
        parent_stub ? parent_stub->decoder_.get() : NULL;
    result = decoder_->SetParent(parent_decoder, parent_texture_id);
  } else {
    // If we don't have a scheduler, it means that Initialize hasn't been called
    // yet. Keep around the requested parent stub and texture so that we can set
    // it in Initialize().
    parent_stub_for_initialization_ = parent_stub ?
        parent_stub->AsWeakPtr() : base::WeakPtr<GpuCommandBufferStub>();
    parent_texture_for_initialization_ = parent_texture_id;
  }
  GpuCommandBufferMsg_SetParent::WriteReplyParams(reply_message, result);
  Send(reply_message);
}

void GpuCommandBufferStub::OnGetState(IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnGetState");
  if (command_buffer_.get()) {
    gpu::CommandBuffer::State state = command_buffer_->GetState();
    if (state.error == gpu::error::kLostContext &&
        gfx::GLContext::LosesAllContextsOnContextLost())
      channel_->LoseAllContexts();

    GpuCommandBufferMsg_GetState::WriteReplyParams(reply_message, state);
  } else {
    DLOG(ERROR) << "no command_buffer.";
    reply_message->set_reply_error();
  }
  Send(reply_message);
}

void GpuCommandBufferStub::OnParseError() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnParseError");
  DCHECK(command_buffer_.get());
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  IPC::Message* msg = new GpuCommandBufferMsg_Destroyed(
      route_id_, state.context_lost_reason);
  msg->set_unblock(true);
  Send(msg);

  // Tell the browser about this context loss as well, so it can
  // determine whether client APIs like WebGL need to be immediately
  // blocked from automatically running.
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->Send(new GpuHostMsg_DidLoseContext(
      handle_.is_null(), state.context_lost_reason, active_url_));
}

void GpuCommandBufferStub::OnGetStateFast(IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnGetStateFast");
  DCHECK(command_buffer_.get());
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (state.error == gpu::error::kLostContext &&
      gfx::GLContext::LosesAllContextsOnContextLost())
    channel_->LoseAllContexts();

  GpuCommandBufferMsg_GetStateFast::WriteReplyParams(reply_message, state);
  Send(reply_message);
}

void GpuCommandBufferStub::OnAsyncFlush(int32 put_offset,
                                        uint32 flush_count) {
  TRACE_EVENT1("gpu", "GpuCommandBufferStub::OnAsyncFlush",
               "put_offset", put_offset);
  DCHECK(command_buffer_.get());
  if (flush_count - last_flush_count_ < 0x8000000U) {
    last_flush_count_ = flush_count;
    command_buffer_->Flush(put_offset);
  } else {
    // We received this message out-of-order. This should not happen but is here
    // to catch regressions. Ignore the message.
    NOTREACHED() << "Received a Flush message out-of-order";
  }

  ReportState();
}

void GpuCommandBufferStub::OnRescheduled() {
  gpu::CommandBuffer::State pre_state = command_buffer_->GetLastState();
  command_buffer_->Flush(pre_state.put_offset);
  gpu::CommandBuffer::State post_state = command_buffer_->GetLastState();

  if (pre_state.get_offset != post_state.get_offset)
    ReportState();
}

void GpuCommandBufferStub::OnRegisterTransferBuffer(
    int32 id,
    base::SharedMemoryHandle transfer_buffer,
    uint32 size) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnRegisterTransferBuffer");
  base::SharedMemory shared_memory(transfer_buffer, false);

  if (command_buffer_.get())
    command_buffer_->RegisterTransferBuffer(id, &shared_memory, size);
}

void GpuCommandBufferStub::OnDestroyTransferBuffer(int32 id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDestroyTransferBuffer");

  if (command_buffer_.get())
    command_buffer_->DestroyTransferBuffer(id);
}

void GpuCommandBufferStub::OnGetTransferBuffer(
    int32 id,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnGetTransferBuffer");
  if (command_buffer_.get()) {
    base::SharedMemoryHandle transfer_buffer = base::SharedMemoryHandle();
    uint32 size = 0;

    gpu::Buffer buffer = command_buffer_->GetTransferBuffer(id);
    if (buffer.shared_memory) {
#if defined(OS_WIN)
      transfer_buffer = NULL;
      BrokerDuplicateHandle(buffer.shared_memory->handle(),
          channel_->renderer_pid(), &transfer_buffer, FILE_MAP_READ |
          FILE_MAP_WRITE, 0);
      DCHECK(transfer_buffer != NULL);
#else
      buffer.shared_memory->ShareToProcess(channel_->renderer_pid(),
                                           &transfer_buffer);
#endif
      size = buffer.size;
    }

    GpuCommandBufferMsg_GetTransferBuffer::WriteReplyParams(reply_message,
                                                            transfer_buffer,
                                                            size);
  } else {
    reply_message->set_reply_error();
  }
  Send(reply_message);
}

void GpuCommandBufferStub::OnCommandProcessed() {
  if (watchdog_)
    watchdog_->CheckArmed();
}

void GpuCommandBufferStub::ReportState() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (state.error == gpu::error::kLostContext &&
      gfx::GLContext::LosesAllContextsOnContextLost()) {
    channel_->LoseAllContexts();
  } else {
    command_buffer_->UpdateState();
  }
}

void GpuCommandBufferStub::PutChanged() {
  FastSetActiveURL(active_url_, active_url_hash_);
  scheduler_->PutChanged();
}

void GpuCommandBufferStub::OnCreateVideoDecoder(
    media::VideoCodecProfile profile,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnCreateVideoDecoder");
  int decoder_route_id = channel_->GenerateRouteID();
  GpuVideoDecodeAccelerator* decoder =
      new GpuVideoDecodeAccelerator(this, decoder_route_id, this);
  video_decoders_.AddWithID(decoder, decoder_route_id);
  channel_->AddRoute(decoder_route_id, decoder);
  decoder->Initialize(profile, reply_message);
}

void GpuCommandBufferStub::OnDestroyVideoDecoder(int decoder_route_id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDestroyVideoDecoder");
  channel_->RemoveRoute(decoder_route_id);
  video_decoders_.Remove(decoder_route_id);
}

void GpuCommandBufferStub::OnSetSurfaceVisible(bool visible) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnSetSurfaceVisible");
  if (memory_manager_client_state_.get())
    memory_manager_client_state_->SetVisible(visible);
}

void GpuCommandBufferStub::OnDiscardBackbuffer() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDiscardBackbuffer");
  if (!surface_)
    return;
  if (surface_->DeferDraws()) {
    DCHECK(!IsScheduled());
    channel_->RequeueMessage();
  } else {
    surface_->SetBackbufferAllocation(false);
  }
}

void GpuCommandBufferStub::OnEnsureBackbuffer() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnEnsureBackbuffer");
  if (!surface_)
    return;
  if (surface_->DeferDraws()) {
    DCHECK(!IsScheduled());
    channel_->RequeueMessage();
  } else {
    surface_->SetBackbufferAllocation(true);
  }
}

void GpuCommandBufferStub::AddSyncPoint(uint32 sync_point) {
  sync_points_.push_back(sync_point);
}

void GpuCommandBufferStub::OnRetireSyncPoint(uint32 sync_point) {
  DCHECK(!sync_points_.empty() && sync_points_.front() == sync_point);
  sync_points_.pop_front();
  GpuChannelManager* manager = channel_->gpu_channel_manager();
  manager->sync_point_manager()->RetireSyncPoint(sync_point);
}

void GpuCommandBufferStub::OnWaitSyncPoint(uint32 sync_point) {
  if (sync_point_wait_count_ == 0) {
    TRACE_EVENT_ASYNC_BEGIN1("gpu", "WaitSyncPoint", this,
                             "GpuCommandBufferStub", this);
  }
  ++sync_point_wait_count_;
  GpuChannelManager* manager = channel_->gpu_channel_manager();
  manager->sync_point_manager()->AddSyncPointCallback(
      sync_point,
      base::Bind(&GpuCommandBufferStub::OnSyncPointRetired,
                 this->AsWeakPtr()));
}

void GpuCommandBufferStub::OnSyncPointRetired() {
  --sync_point_wait_count_;
  if (sync_point_wait_count_ == 0) {
    TRACE_EVENT_ASYNC_END1("gpu", "WaitSyncPoint", this,
                           "GpuCommandBufferStub", this);
  }
  OnReschedule();
}

void GpuCommandBufferStub::OnSignalSyncPoint(uint32 sync_point, uint32 id) {
  GpuChannelManager* manager = channel_->gpu_channel_manager();
  manager->sync_point_manager()->AddSyncPointCallback(
      sync_point,
      base::Bind(&GpuCommandBufferStub::OnSignalSyncPointAck,
                 this->AsWeakPtr(),
                 id));
}

void GpuCommandBufferStub::OnSignalSyncPointAck(uint32 id) {
  Send(new GpuCommandBufferMsg_SignalSyncPointAck(route_id_, id));
}

void GpuCommandBufferStub::OnReceivedClientManagedMemoryStats(
    const GpuManagedMemoryStats& stats) {
  TRACE_EVENT0(
      "gpu",
      "GpuCommandBufferStub::OnReceivedClientManagedMemoryStats");
  if (memory_manager_client_state_.get())
    memory_manager_client_state_->SetManagedMemoryStats(stats);
}

void GpuCommandBufferStub::OnSetClientHasMemoryAllocationChangedCallback(
    bool has_callback) {
  TRACE_EVENT0(
      "gpu",
      "GpuCommandBufferStub::OnSetClientHasMemoryAllocationChangedCallback");
  if (has_callback) {
    if (!memory_manager_client_state_.get()) {
      memory_manager_client_state_.reset(GetMemoryManager()->CreateClientState(
          this, surface_id_ != 0, true));
    }
  } else {
    memory_manager_client_state_.reset();
  }
}

void GpuCommandBufferStub::SendConsoleMessage(
    int32 id,
    const std::string& message) {
  GPUCommandBufferConsoleMessage console_message;
  console_message.id = id;
  console_message.message = message;
  IPC::Message* msg = new GpuCommandBufferMsg_ConsoleMsg(
      route_id_, console_message);
  msg->set_unblock(true);
  Send(msg);
}

void GpuCommandBufferStub::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void GpuCommandBufferStub::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

void GpuCommandBufferStub::SetPreemptByCounter(
    scoped_refptr<gpu::RefCountedCounter> counter) {
  preempt_by_counter_ = counter;
  if (scheduler_.get())
    scheduler_->SetPreemptByCounter(preempt_by_counter_);
}

bool GpuCommandBufferStub::GetTotalGpuMemory(size_t* bytes) {
  *bytes = total_gpu_memory_;
  return !!total_gpu_memory_;
}

gfx::Size GpuCommandBufferStub::GetSurfaceSize() const {
  if (!surface_)
    return gfx::Size();
  return surface_->GetSize();
}

gpu::gles2::MemoryTracker* GpuCommandBufferStub::GetMemoryTracker() const {
  return context_group_->memory_tracker();
}

void GpuCommandBufferStub::SetMemoryAllocation(
    const GpuMemoryAllocation& allocation) {
  Send(new GpuCommandBufferMsg_SetMemoryAllocation(
      route_id_, allocation.renderer_allocation));
  // This can be called outside of OnMessageReceived, so the context needs to be
  // made current before calling methods on the surface.
  if (!surface_ || !MakeCurrent())
    return;
  surface_->SetFrontbufferAllocation(
      allocation.browser_allocation.suggest_have_frontbuffer);
}

}  // namespace content
