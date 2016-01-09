// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_command_buffer_stub.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/hash.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_watchdog.h"
#include "content/common/gpu/image_transport_surface.h"
#include "content/common/gpu/media/gpu_video_decode_accelerator.h"
#include "content/common/gpu/media/gpu_video_encode_accelerator.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "content/public/common/sandbox_init.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/stream_texture_android.h"
#endif

namespace content {
struct WaitForCommandState {
  WaitForCommandState(int32_t start, int32_t end, IPC::Message* reply)
      : start(start), end(end), reply(reply) {}

  int32_t start;
  int32_t end;
  scoped_ptr<IPC::Message> reply;
};

namespace {

// The GpuCommandBufferMemoryTracker class provides a bridge between the
// ContextGroup's memory type managers and the GpuMemoryManager class.
class GpuCommandBufferMemoryTracker : public gpu::gles2::MemoryTracker {
 public:
  explicit GpuCommandBufferMemoryTracker(GpuChannel* channel,
                                         uint64_t share_group_tracing_guid)
      : tracking_group_(
            channel->gpu_channel_manager()
                ->gpu_memory_manager()
                ->CreateTrackingGroup(channel->GetClientPID(), this)),
        client_tracing_id_(channel->client_tracing_id()),
        client_id_(channel->client_id()),
        share_group_tracing_guid_(share_group_tracing_guid) {}

  void TrackMemoryAllocatedChange(
      size_t old_size, size_t new_size) override {
    tracking_group_->TrackMemoryAllocatedChange(
        old_size, new_size);
  }

  bool EnsureGPUMemoryAvailable(size_t size_needed) override {
    return tracking_group_->EnsureGPUMemoryAvailable(size_needed);
  };

  uint64_t ClientTracingId() const override { return client_tracing_id_; }
  int ClientId() const override { return client_id_; }
  uint64_t ShareGroupTracingGUID() const override {
    return share_group_tracing_guid_;
  }

 private:
  ~GpuCommandBufferMemoryTracker() override {}
  scoped_ptr<GpuMemoryTrackingGroup> tracking_group_;
  const uint64_t client_tracing_id_;
  const int client_id_;
  const uint64_t share_group_tracing_guid_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferMemoryTracker);
};

// FastSetActiveURL will shortcut the expensive call to SetActiveURL when the
// url_hash matches.
void FastSetActiveURL(const GURL& url, size_t url_hash) {
  // Leave the previously set URL in the empty case -- empty URLs are given by
  // BlinkPlatformImpl::createOffscreenGraphicsContext3D. Hopefully the
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
const int64_t kHandleMoreWorkPeriodMs = 2;
const int64_t kHandleMoreWorkPeriodBusyMs = 1;

// Prevents idle work from being starved.
const int64_t kMaxTimeSinceIdleMs = 10;

class DevToolsChannelData : public base::trace_event::ConvertableToTraceFormat {
 public:
  static scoped_refptr<base::trace_event::ConvertableToTraceFormat>
  CreateForChannel(GpuChannel* channel);

  void AppendAsTraceFormat(std::string* out) const override {
    std::string tmp;
    base::JSONWriter::Write(*value_, &tmp);
    *out += tmp;
  }

 private:
  explicit DevToolsChannelData(base::Value* value) : value_(value) {}
  ~DevToolsChannelData() override {}
  scoped_ptr<base::Value> value_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsChannelData);
};

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
DevToolsChannelData::CreateForChannel(GpuChannel* channel) {
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue);
  res->SetInteger("renderer_pid", channel->GetClientPID());
  res->SetDouble("used_bytes", channel->GetMemoryUsage());
  return new DevToolsChannelData(res.release());
}

void RunOnThread(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 const base::Closure& callback) {
  if (task_runner->BelongsToCurrentThread()) {
    callback.Run();
  } else {
    task_runner->PostTask(FROM_HERE, callback);
  }
}

uint64_t GetCommandBufferID(int channel_id, int32_t route_id) {
  return (static_cast<uint64_t>(channel_id) << 32) | route_id;
}

}  // namespace

GpuCommandBufferStub::GpuCommandBufferStub(
    GpuChannel* channel,
    gpu::SyncPointManager* sync_point_manager,
    base::SingleThreadTaskRunner* task_runner,
    GpuCommandBufferStub* share_group,
    const gfx::GLSurfaceHandle& handle,
    gpu::gles2::MailboxManager* mailbox_manager,
    gpu::PreemptionFlag* preempt_by_flag,
    gpu::gles2::SubscriptionRefSet* subscription_ref_set,
    gpu::ValueStateMap* pending_valuebuffer_state,
    const gfx::Size& size,
    const gpu::gles2::DisallowedFeatures& disallowed_features,
    const std::vector<int32_t>& attribs,
    gfx::GpuPreference gpu_preference,
    int32_t stream_id,
    int32_t route_id,
    bool offscreen,
    GpuWatchdog* watchdog,
    const GURL& active_url)
    : channel_(channel),
      sync_point_manager_(sync_point_manager),
      task_runner_(task_runner),
      initialized_(false),
      handle_(handle),
      initial_size_(size),
      disallowed_features_(disallowed_features),
      requested_attribs_(attribs),
      gpu_preference_(gpu_preference),
      use_virtualized_gl_context_(false),
      command_buffer_id_(GetCommandBufferID(channel->client_id(), route_id)),
      stream_id_(stream_id),
      route_id_(route_id),
      offscreen_(offscreen),
      last_flush_count_(0),
      watchdog_(watchdog),
      waiting_for_sync_point_(false),
      previous_processed_num_(0),
      preemption_flag_(preempt_by_flag),
      active_url_(active_url) {
  active_url_hash_ = base::Hash(active_url.possibly_invalid_spec());
  FastSetActiveURL(active_url_, active_url_hash_);

  gpu::gles2::ContextCreationAttribHelper attrib_parser;
  attrib_parser.Parse(requested_attribs_);

  if (share_group) {
    context_group_ = share_group->context_group_;
    DCHECK(context_group_->bind_generates_resource() ==
           attrib_parser.bind_generates_resource);
  } else {
    context_group_ = new gpu::gles2::ContextGroup(
        mailbox_manager,
        new GpuCommandBufferMemoryTracker(channel, command_buffer_id_),
        channel_->gpu_channel_manager()->shader_translator_cache(),
        channel_->gpu_channel_manager()->framebuffer_completeness_cache(), NULL,
        subscription_ref_set, pending_valuebuffer_state,
        attrib_parser.bind_generates_resource);
  }

// Virtualize PreferIntegratedGpu contexts by default on OS X to prevent
// performance regressions when enabling FCM.
// http://crbug.com/180463
#if defined(OS_MACOSX)
  if (gpu_preference_ == gfx::PreferIntegratedGpu)
    use_virtualized_gl_context_ = true;
#endif

  use_virtualized_gl_context_ |=
      context_group_->feature_info()->workarounds().use_virtualized_gl_contexts;

  // MailboxManagerSync synchronization correctness currently depends on having
  // only a single context. See crbug.com/510243 for details.
  use_virtualized_gl_context_ |= mailbox_manager->UsesSync();

  if (offscreen && initial_size_.IsEmpty()) {
    // If we're an offscreen surface with zero width and/or height, set to a
    // non-zero size so that we have a complete framebuffer for operations like
    // glClear.
    initial_size_ = gfx::Size(1, 1);
  }
}

GpuCommandBufferStub::~GpuCommandBufferStub() {
  Destroy();
}

GpuMemoryManager* GpuCommandBufferStub::GetMemoryManager() const {
    return channel()->gpu_channel_manager()->gpu_memory_manager();
}

bool GpuCommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "GPUTask",
               "data",
               DevToolsChannelData::CreateForChannel(channel()));
  FastSetActiveURL(active_url_, active_url_hash_);

  bool have_context = false;
  // Ensure the appropriate GL context is current before handling any IPC
  // messages directed at the command buffer. This ensures that the message
  // handler can assume that the context is current (not necessary for
  // RetireSyncPoint or WaitSyncPoint).
  if (decoder_.get() &&
      message.type() != GpuCommandBufferMsg_SetGetBuffer::ID &&
      message.type() != GpuCommandBufferMsg_WaitForTokenInRange::ID &&
      message.type() != GpuCommandBufferMsg_WaitForGetOffsetInRange::ID &&
      message.type() != GpuCommandBufferMsg_RegisterTransferBuffer::ID &&
      message.type() != GpuCommandBufferMsg_DestroyTransferBuffer::ID &&
      message.type() != GpuCommandBufferMsg_RetireSyncPoint::ID &&
      message.type() != GpuCommandBufferMsg_SignalSyncPoint::ID) {
    if (!MakeCurrent())
      return false;
    have_context = true;
  }

  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuCommandBufferStub, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_Initialize,
                                    OnInitialize);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_SetGetBuffer,
                                    OnSetGetBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ProduceFrontBuffer,
                        OnProduceFrontBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_WaitForTokenInRange,
                                    OnWaitForTokenInRange);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_WaitForGetOffsetInRange,
                                    OnWaitForGetOffsetInRange);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RegisterTransferBuffer,
                        OnRegisterTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyTransferBuffer,
                        OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateVideoDecoder,
                                    OnCreateVideoDecoder)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateVideoEncoder,
                                    OnCreateVideoEncoder)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RetireSyncPoint,
                        OnRetireSyncPoint)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalSyncPoint,
                        OnSignalSyncPoint)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalSyncToken,
                        OnSignalSyncToken)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalQuery,
                        OnSignalQuery)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateImage, OnCreateImage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyImage, OnDestroyImage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateStreamTexture,
                        OnCreateStreamTexture)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  CheckCompleteWaits();

  // Ensure that any delayed work that was created will be handled.
  if (have_context) {
    if (scheduler_)
      scheduler_->ProcessPendingQueries();
    ScheduleDelayedWork(
        base::TimeDelta::FromMilliseconds(kHandleMoreWorkPeriodMs));
  }

  DCHECK(handled);
  return handled;
}

bool GpuCommandBufferStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}

bool GpuCommandBufferStub::IsScheduled() {
  return (!scheduler_.get() || scheduler_->scheduled());
}

void GpuCommandBufferStub::PollWork() {
  // Post another delayed task if we have not yet reached the time at which
  // we should process delayed work.
  base::TimeTicks current_time = base::TimeTicks::Now();
  DCHECK(!process_delayed_work_time_.is_null());
  if (process_delayed_work_time_ > current_time) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&GpuCommandBufferStub::PollWork, AsWeakPtr()),
        process_delayed_work_time_ - current_time);
    return;
  }
  process_delayed_work_time_ = base::TimeTicks();

  PerformWork();
}

void GpuCommandBufferStub::PerformWork() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::PerformWork");

  FastSetActiveURL(active_url_, active_url_hash_);
  if (decoder_.get() && !MakeCurrent())
    return;

  if (scheduler_) {
    uint32_t current_unprocessed_num =
        channel()->gpu_channel_manager()->GetUnprocessedOrderNum();
    // We're idle when no messages were processed or scheduled.
    bool is_idle = (previous_processed_num_ == current_unprocessed_num);
    if (!is_idle && !last_idle_time_.is_null()) {
      base::TimeDelta time_since_idle =
          base::TimeTicks::Now() - last_idle_time_;
      base::TimeDelta max_time_since_idle =
          base::TimeDelta::FromMilliseconds(kMaxTimeSinceIdleMs);

      // Force idle when it's been too long since last time we were idle.
      if (time_since_idle > max_time_since_idle)
        is_idle = true;
    }

    if (is_idle) {
      last_idle_time_ = base::TimeTicks::Now();
      scheduler_->PerformIdleWork();
    }

    scheduler_->ProcessPendingQueries();
  }

  ScheduleDelayedWork(
      base::TimeDelta::FromMilliseconds(kHandleMoreWorkPeriodBusyMs));
}

bool GpuCommandBufferStub::HasUnprocessedCommands() {
  if (command_buffer_) {
    gpu::CommandBuffer::State state = command_buffer_->GetLastState();
    return command_buffer_->GetPutOffset() != state.get_offset &&
        !gpu::error::IsError(state.error);
  }
  return false;
}

void GpuCommandBufferStub::ScheduleDelayedWork(base::TimeDelta delay) {
  bool has_more_work = scheduler_.get() && (scheduler_->HasPendingQueries() ||
                                            scheduler_->HasMoreIdleWork());
  if (!has_more_work) {
    last_idle_time_ = base::TimeTicks();
    return;
  }

  base::TimeTicks current_time = base::TimeTicks::Now();
  // |process_delayed_work_time_| is set if processing of delayed work is
  // already scheduled. Just update the time if already scheduled.
  if (!process_delayed_work_time_.is_null()) {
    process_delayed_work_time_ = current_time + delay;
    return;
  }

  // Idle when no messages are processed between now and when
  // PollWork is called.
  previous_processed_num_ =
      channel()->gpu_channel_manager()->GetProcessedOrderNum();
  if (last_idle_time_.is_null())
    last_idle_time_ = current_time;

  // IsScheduled() returns true after passing all unschedule fences
  // and this is when we can start performing idle work. Idle work
  // is done synchronously so we can set delay to 0 and instead poll
  // for more work at the rate idle work is performed. This also ensures
  // that idle work is done as efficiently as possible without any
  // unnecessary delays.
  if (scheduler_.get() && scheduler_->scheduled() &&
      scheduler_->HasMoreIdleWork()) {
    delay = base::TimeDelta();
  }

  process_delayed_work_time_ = current_time + delay;
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&GpuCommandBufferStub::PollWork, AsWeakPtr()),
      delay);
}

bool GpuCommandBufferStub::MakeCurrent() {
  if (decoder_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
  command_buffer_->SetParseError(gpu::error::kLostContext);
  CheckContextLost();
  return false;
}

void GpuCommandBufferStub::Destroy() {
  if (wait_for_token_) {
    Send(wait_for_token_->reply.release());
    wait_for_token_.reset();
  }
  if (wait_for_get_offset_) {
    Send(wait_for_get_offset_->reply.release());
    wait_for_get_offset_.reset();
  }

  if (initialized_) {
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    if (handle_.is_null() && !active_url_.is_empty()) {
      gpu_channel_manager->Send(
          new GpuHostMsg_DidDestroyOffscreenContext(active_url_));
    }
  }

  while (!sync_points_.empty())
    OnRetireSyncPoint(sync_points_.front());

  if (decoder_)
    decoder_->set_engine(NULL);

  // The scheduler has raw references to the decoder and the command buffer so
  // destroy it before those.
  scheduler_.reset();

  sync_point_client_.reset();

  bool have_context = false;
  if (decoder_ && decoder_->GetGLContext()) {
    // Try to make the context current regardless of whether it was lost, so we
    // don't leak resources.
    have_context = decoder_->GetGLContext()->MakeCurrent(surface_.get());
  }
  FOR_EACH_OBSERVER(DestructionObserver,
                    destruction_observers_,
                    OnWillDestroyStub());

  if (decoder_) {
    decoder_->Destroy(have_context);
    decoder_.reset();
  }

  command_buffer_.reset();

  // Remove this after crbug.com/248395 is sorted out.
  surface_ = NULL;
}

void GpuCommandBufferStub::OnInitializeFailed(IPC::Message* reply_message) {
  Destroy();
  GpuCommandBufferMsg_Initialize::WriteReplyParams(
      reply_message, false, gpu::Capabilities());
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

  bool result = command_buffer_->Initialize();
  DCHECK(result);

  GpuChannelManager* manager = channel_->gpu_channel_manager();
  DCHECK(manager);

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group_.get()));
  scheduler_.reset(new gpu::GpuScheduler(command_buffer_.get(),
                                         decoder_.get(),
                                         decoder_.get()));
  sync_point_client_ = sync_point_manager_->CreateSyncPointClient(
      channel_->GetSyncPointOrderData(), gpu::CommandBufferNamespace::GPU_IO,
      command_buffer_id_);

  if (preemption_flag_.get())
    scheduler_->SetPreemptByFlag(preemption_flag_);

  decoder_->set_engine(scheduler_.get());

  if (!handle_.is_null()) {
    surface_ = ImageTransportSurface::CreateSurface(
        channel_->gpu_channel_manager(),
        this,
        handle_);
  } else {
    surface_ = manager->GetDefaultOffscreenSurface();
  }

  if (!surface_.get()) {
    DLOG(ERROR) << "Failed to create surface.";
    OnInitializeFailed(reply_message);
    return;
  }

  scoped_refptr<gfx::GLContext> context;
  if (use_virtualized_gl_context_ && channel_->share_group()) {
    context = channel_->share_group()->GetSharedContext();
    if (!context.get()) {
      context = gfx::GLContext::CreateGLContext(
          channel_->share_group(),
          channel_->gpu_channel_manager()->GetDefaultOffscreenSurface(),
          gpu_preference_);
      if (!context.get()) {
        DLOG(ERROR) << "Failed to create shared context for virtualization.";
        OnInitializeFailed(reply_message);
        return;
      }
      channel_->share_group()->SetSharedContext(context.get());
    }
    // This should be a non-virtual GL context.
    DCHECK(context->GetHandle());
    context = new gpu::GLContextVirtual(
        channel_->share_group(), context.get(), decoder_->AsWeakPtr());
    if (!context->Initialize(surface_.get(), gpu_preference_)) {
      // TODO(sievers): The real context created above for the default
      // offscreen surface might not be compatible with this surface.
      // Need to adjust at least GLX to be able to create the initial context
      // with a config that is compatible with onscreen and offscreen surfaces.
      context = NULL;

      DLOG(ERROR) << "Failed to initialize virtual GL context.";
      OnInitializeFailed(reply_message);
      return;
    }
  }
  if (!context.get()) {
    context = gfx::GLContext::CreateGLContext(
        channel_->share_group(), surface_.get(), gpu_preference_);
  }
  if (!context.get()) {
    DLOG(ERROR) << "Failed to create context.";
    OnInitializeFailed(reply_message);
    return;
  }

  if (!context->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Failed to make context current.";
    OnInitializeFailed(reply_message);
    return;
  }

  if (!context->GetGLStateRestorer()) {
    context->SetGLStateRestorer(
        new gpu::GLStateRestorerImpl(decoder_->AsWeakPtr()));
  }

  if (!context_group_->has_program_cache() &&
      !context_group_->feature_info()->workarounds().disable_program_cache) {
    context_group_->set_program_cache(
        channel_->gpu_channel_manager()->program_cache());
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  if (!decoder_->Initialize(surface_, context, offscreen_, initial_size_,
                            disallowed_features_, requested_attribs_)) {
    DLOG(ERROR) << "Failed to initialize decoder.";
    OnInitializeFailed(reply_message);
    return;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGPUServiceLogging)) {
    decoder_->set_log_commands(true);
  }

  decoder_->GetLogger()->SetMsgCallback(
      base::Bind(&GpuCommandBufferStub::SendConsoleMessage,
                 base::Unretained(this)));
  decoder_->SetShaderCacheCallback(
      base::Bind(&GpuCommandBufferStub::SendCachedShader,
                 base::Unretained(this)));
  decoder_->SetWaitSyncPointCallback(
      base::Bind(&GpuCommandBufferStub::OnWaitSyncPoint,
                 base::Unretained(this)));
  decoder_->SetFenceSyncReleaseCallback(base::Bind(
      &GpuCommandBufferStub::OnFenceSyncRelease, base::Unretained(this)));
  decoder_->SetWaitFenceSyncCallback(base::Bind(
      &GpuCommandBufferStub::OnWaitFenceSync, base::Unretained(this)));

  command_buffer_->SetPutOffsetChangeCallback(
      base::Bind(&GpuCommandBufferStub::PutChanged, base::Unretained(this)));
  command_buffer_->SetGetBufferChangeCallback(
      base::Bind(&gpu::GpuScheduler::SetGetBuffer,
                 base::Unretained(scheduler_.get())));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&GpuCommandBufferStub::OnParseError, base::Unretained(this)));
  scheduler_->SetSchedulingChangedCallback(base::Bind(
      &GpuCommandBufferStub::OnSchedulingChanged, base::Unretained(this)));

  if (watchdog_) {
    scheduler_->SetCommandProcessedCallback(
        base::Bind(&GpuCommandBufferStub::OnCommandProcessed,
                   base::Unretained(this)));
  }

  const size_t kSharedStateSize = sizeof(gpu::CommandBufferSharedState);
  if (!shared_state_shm->Map(kSharedStateSize)) {
    DLOG(ERROR) << "Failed to map shared state buffer.";
    OnInitializeFailed(reply_message);
    return;
  }
  command_buffer_->SetSharedStateBuffer(gpu::MakeBackingFromSharedMemory(
      std::move(shared_state_shm), kSharedStateSize));

  gpu::Capabilities capabilities = decoder_->GetCapabilities();
  capabilities.future_sync_points = channel_->allow_future_sync_points();

  GpuCommandBufferMsg_Initialize::WriteReplyParams(
      reply_message, true, capabilities);
  Send(reply_message);

  if (handle_.is_null() && !active_url_.is_empty()) {
    manager->Send(new GpuHostMsg_DidCreateOffscreenContext(
        active_url_));
  }

  initialized_ = true;
}

void GpuCommandBufferStub::OnCreateStreamTexture(uint32_t texture_id,
                                                 int32_t stream_id,
                                                 bool* succeeded) {
#if defined(OS_ANDROID)
  *succeeded = StreamTexture::Create(this, texture_id, stream_id);
#else
  *succeeded = false;
#endif
}

void GpuCommandBufferStub::SetLatencyInfoCallback(
    const LatencyInfoCallback& callback) {
  latency_info_callback_ = callback;
}

int32_t GpuCommandBufferStub::GetRequestedAttribute(int attr) const {
  // The command buffer is pairs of enum, value
  // search for the requested attribute, return the value.
  for (std::vector<int32_t>::const_iterator it = requested_attribs_.begin();
       it != requested_attribs_.end(); ++it) {
    if (*it++ == attr) {
      return *it;
    }
  }
  return -1;
}

void GpuCommandBufferStub::OnSetGetBuffer(int32_t shm_id,
                                          IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnSetGetBuffer");
  if (command_buffer_)
    command_buffer_->SetGetBuffer(shm_id);
  Send(reply_message);
}

void GpuCommandBufferStub::OnProduceFrontBuffer(const gpu::Mailbox& mailbox) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnProduceFrontBuffer");
  if (!decoder_) {
    LOG(ERROR) << "Can't produce front buffer before initialization.";
    return;
  }

  decoder_->ProduceFrontBuffer(mailbox);
}

void GpuCommandBufferStub::OnParseError() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnParseError");
  DCHECK(command_buffer_.get());
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  IPC::Message* msg = new GpuCommandBufferMsg_Destroyed(
      route_id_, state.context_lost_reason, state.error);
  msg->set_unblock(true);
  Send(msg);

  // Tell the browser about this context loss as well, so it can
  // determine whether client APIs like WebGL need to be immediately
  // blocked from automatically running.
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->Send(new GpuHostMsg_DidLoseContext(
      handle_.is_null(), state.context_lost_reason, active_url_));

  CheckContextLost();
}

void GpuCommandBufferStub::OnSchedulingChanged(bool scheduled) {
  TRACE_EVENT1("gpu", "GpuCommandBufferStub::OnSchedulingChanged", "scheduled",
               scheduled);
  channel_->OnStubSchedulingChanged(this, scheduled);
}

void GpuCommandBufferStub::OnWaitForTokenInRange(int32_t start,
                                                 int32_t end,
                                                 IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnWaitForTokenInRange");
  DCHECK(command_buffer_.get());
  CheckContextLost();
  if (wait_for_token_)
    LOG(ERROR) << "Got WaitForToken command while currently waiting for token.";
  wait_for_token_ =
      make_scoped_ptr(new WaitForCommandState(start, end, reply_message));
  CheckCompleteWaits();
}

void GpuCommandBufferStub::OnWaitForGetOffsetInRange(
    int32_t start,
    int32_t end,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnWaitForGetOffsetInRange");
  DCHECK(command_buffer_.get());
  CheckContextLost();
  if (wait_for_get_offset_) {
    LOG(ERROR)
        << "Got WaitForGetOffset command while currently waiting for offset.";
  }
  wait_for_get_offset_ =
      make_scoped_ptr(new WaitForCommandState(start, end, reply_message));
  CheckCompleteWaits();
}

void GpuCommandBufferStub::CheckCompleteWaits() {
  if (wait_for_token_ || wait_for_get_offset_) {
    gpu::CommandBuffer::State state = command_buffer_->GetLastState();
    if (wait_for_token_ &&
        (gpu::CommandBuffer::InRange(
             wait_for_token_->start, wait_for_token_->end, state.token) ||
         state.error != gpu::error::kNoError)) {
      ReportState();
      GpuCommandBufferMsg_WaitForTokenInRange::WriteReplyParams(
          wait_for_token_->reply.get(), state);
      Send(wait_for_token_->reply.release());
      wait_for_token_.reset();
    }
    if (wait_for_get_offset_ &&
        (gpu::CommandBuffer::InRange(wait_for_get_offset_->start,
                                     wait_for_get_offset_->end,
                                     state.get_offset) ||
         state.error != gpu::error::kNoError)) {
      ReportState();
      GpuCommandBufferMsg_WaitForGetOffsetInRange::WriteReplyParams(
          wait_for_get_offset_->reply.get(), state);
      Send(wait_for_get_offset_->reply.release());
      wait_for_get_offset_.reset();
    }
  }
}

void GpuCommandBufferStub::OnAsyncFlush(
    int32_t put_offset,
    uint32_t flush_count,
    const std::vector<ui::LatencyInfo>& latency_info) {
  TRACE_EVENT1(
      "gpu", "GpuCommandBufferStub::OnAsyncFlush", "put_offset", put_offset);
  DCHECK(command_buffer_);

  // We received this message out-of-order. This should not happen but is here
  // to catch regressions. Ignore the message.
  DVLOG_IF(0, flush_count - last_flush_count_ >= 0x8000000U)
      << "Received a Flush message out-of-order";

  if (flush_count > last_flush_count_ &&
      ui::LatencyInfo::Verify(latency_info,
                              "GpuCommandBufferStub::OnAsyncFlush") &&
      !latency_info_callback_.is_null()) {
    latency_info_callback_.Run(latency_info);
  }

  last_flush_count_ = flush_count;
  gpu::CommandBuffer::State pre_state = command_buffer_->GetLastState();
  command_buffer_->Flush(put_offset);
  gpu::CommandBuffer::State post_state = command_buffer_->GetLastState();

  if (pre_state.get_offset != post_state.get_offset)
    ReportState();

#if defined(OS_ANDROID)
  GpuChannelManager* manager = channel_->gpu_channel_manager();
  manager->DidAccessGpu();
#endif
}

void GpuCommandBufferStub::OnRegisterTransferBuffer(
    int32_t id,
    base::SharedMemoryHandle transfer_buffer,
    uint32_t size) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnRegisterTransferBuffer");

  // Take ownership of the memory and map it into this process.
  // This validates the size.
  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(transfer_buffer, false));
  if (!shared_memory->Map(size)) {
    DVLOG(0) << "Failed to map shared memory.";
    return;
  }

  if (command_buffer_) {
    command_buffer_->RegisterTransferBuffer(
        id, gpu::MakeBackingFromSharedMemory(std::move(shared_memory), size));
  }
}

void GpuCommandBufferStub::OnDestroyTransferBuffer(int32_t id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDestroyTransferBuffer");

  if (command_buffer_)
    command_buffer_->DestroyTransferBuffer(id);
}

void GpuCommandBufferStub::OnCommandProcessed() {
  if (watchdog_)
    watchdog_->CheckArmed();
}

void GpuCommandBufferStub::ReportState() { command_buffer_->UpdateState(); }

void GpuCommandBufferStub::PutChanged() {
  FastSetActiveURL(active_url_, active_url_hash_);
  scheduler_->PutChanged();
}

void GpuCommandBufferStub::OnCreateVideoDecoder(
    const media::VideoDecodeAccelerator::Config& config,
    int32_t decoder_route_id,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnCreateVideoDecoder");
  GpuVideoDecodeAccelerator* decoder = new GpuVideoDecodeAccelerator(
      decoder_route_id, this, channel_->io_task_runner());
  decoder->Initialize(config, reply_message);
  // decoder is registered as a DestructionObserver of this stub and will
  // self-delete during destruction of this stub.
}

void GpuCommandBufferStub::OnCreateVideoEncoder(
    media::VideoPixelFormat input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32_t initial_bitrate,
    int32_t encoder_route_id,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnCreateVideoEncoder");
  GpuVideoEncodeAccelerator* encoder =
      new GpuVideoEncodeAccelerator(encoder_route_id, this);
  encoder->Initialize(input_format,
                      input_visible_size,
                      output_profile,
                      initial_bitrate,
                      reply_message);
  // encoder is registered as a DestructionObserver of this stub and will
  // self-delete during destruction of this stub.
}

void GpuCommandBufferStub::InsertSyncPoint(uint32_t sync_point, bool retire) {
  sync_points_.push_back(sync_point);
  if (retire) {
    OnMessageReceived(
        GpuCommandBufferMsg_RetireSyncPoint(route_id_, sync_point));
  }
}

void GpuCommandBufferStub::OnRetireSyncPoint(uint32_t sync_point) {
  DCHECK(!sync_points_.empty() && sync_points_.front() == sync_point);
  sync_points_.pop_front();

  gpu::gles2::MailboxManager* mailbox_manager =
      context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync() && MakeCurrent()) {
    // Old sync points are global and do not have a command buffer ID,
    // We can simply use the global sync point number as the release count with
    // 0 for the command buffer ID (under normal circumstances 0 is invalid so
    // will not be used) until the old sync points are replaced.
    gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO, 0, 0,
                              sync_point);
    mailbox_manager->PushTextureUpdates(sync_token);
  }

  sync_point_manager_->RetireSyncPoint(sync_point);
}

bool GpuCommandBufferStub::OnWaitSyncPoint(uint32_t sync_point) {
  DCHECK(!waiting_for_sync_point_);
  DCHECK(scheduler_->scheduled());
  if (!sync_point)
    return true;
  if (sync_point_manager_->IsSyncPointRetired(sync_point)) {
    // Old sync points are global and do not have a command buffer ID,
    // We can simply use the global sync point number as the release count with
    // 0 for the command buffer ID (under normal circumstances 0 is invalid so
    // will not be used) until the old sync points are replaced.
    PullTextureUpdates(gpu::CommandBufferNamespace::GPU_IO, 0, sync_point);
    return true;
  }

  TRACE_EVENT_ASYNC_BEGIN1("gpu", "WaitSyncPoint", this, "GpuCommandBufferStub",
                           this);

  waiting_for_sync_point_ = true;
  sync_point_manager_->AddSyncPointCallback(
      sync_point,
      base::Bind(&RunOnThread, task_runner_,
                 base::Bind(&GpuCommandBufferStub::OnWaitSyncPointCompleted,
                            this->AsWeakPtr(), sync_point)));

  if (!waiting_for_sync_point_)
    return true;

  scheduler_->SetScheduled(false);
  return false;
}

void GpuCommandBufferStub::OnWaitSyncPointCompleted(uint32_t sync_point) {
  DCHECK(waiting_for_sync_point_);
  TRACE_EVENT_ASYNC_END1("gpu", "WaitSyncPoint", this, "GpuCommandBufferStub",
                         this);
  // Old sync points are global and do not have a command buffer ID,
  // We can simply use the global sync point number as the release count with
  // 0 for the command buffer ID (under normal circumstances 0 is invalid so
  // will not be used) until the old sync points are replaced.
  PullTextureUpdates(gpu::CommandBufferNamespace::GPU_IO, 0, sync_point);
  waiting_for_sync_point_ = false;
  scheduler_->SetScheduled(true);
}

void GpuCommandBufferStub::PullTextureUpdates(
    gpu::CommandBufferNamespace namespace_id,
    uint64_t command_buffer_id,
    uint32_t release) {
  gpu::gles2::MailboxManager* mailbox_manager =
      context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync() && MakeCurrent()) {
    gpu::SyncToken sync_token(namespace_id, 0, command_buffer_id, release);
    mailbox_manager->PullTextureUpdates(sync_token);
  }
}

void GpuCommandBufferStub::OnSignalSyncPoint(uint32_t sync_point, uint32_t id) {
  sync_point_manager_->AddSyncPointCallback(
      sync_point,
      base::Bind(&GpuCommandBufferStub::OnSignalAck, this->AsWeakPtr(), id));
}

void GpuCommandBufferStub::OnSignalSyncToken(const gpu::SyncToken& sync_token,
                                             uint32_t id) {
  scoped_refptr<gpu::SyncPointClientState> release_state =
      sync_point_manager_->GetSyncPointClientState(
          sync_token.namespace_id(), sync_token.command_buffer_id());

  if (release_state) {
    sync_point_client_->Wait(release_state.get(), sync_token.release_count(),
                             base::Bind(&GpuCommandBufferStub::OnSignalAck,
                                        this->AsWeakPtr(), id));
  } else {
    OnSignalAck(id);
  }
}

void GpuCommandBufferStub::OnSignalAck(uint32_t id) {
  Send(new GpuCommandBufferMsg_SignalAck(route_id_, id));
}

void GpuCommandBufferStub::OnSignalQuery(uint32_t query_id, uint32_t id) {
  if (decoder_) {
    gpu::gles2::QueryManager* query_manager = decoder_->GetQueryManager();
    if (query_manager) {
      gpu::gles2::QueryManager::Query* query =
          query_manager->GetQuery(query_id);
      if (query) {
        query->AddCallback(
          base::Bind(&GpuCommandBufferStub::OnSignalAck,
                     this->AsWeakPtr(),
                     id));
        return;
      }
    }
  }
  // Something went wrong, run callback immediately.
  OnSignalAck(id);
}

void GpuCommandBufferStub::OnFenceSyncRelease(uint64_t release) {
  if (sync_point_client_->client_state()->IsFenceSyncReleased(release)) {
    DLOG(ERROR) << "Fence Sync has already been released.";
    return;
  }

  gpu::gles2::MailboxManager* mailbox_manager =
      context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync() && MakeCurrent()) {
    gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO, 0,
                              command_buffer_id_, release);
    mailbox_manager->PushTextureUpdates(sync_token);
  }

  sync_point_client_->ReleaseFenceSync(release);
}

bool GpuCommandBufferStub::OnWaitFenceSync(
    gpu::CommandBufferNamespace namespace_id,
    uint64_t command_buffer_id,
    uint64_t release) {
  DCHECK(!waiting_for_sync_point_);
  DCHECK(scheduler_->scheduled());

  scoped_refptr<gpu::SyncPointClientState> release_state =
      sync_point_manager_->GetSyncPointClientState(namespace_id,
                                                   command_buffer_id);

  if (!release_state)
    return true;

  if (release_state->IsFenceSyncReleased(release)) {
    PullTextureUpdates(namespace_id, command_buffer_id, release);
    return true;
  }

  TRACE_EVENT_ASYNC_BEGIN1("gpu", "WaitFenceSync", this, "GpuCommandBufferStub",
                           this);
  waiting_for_sync_point_ = true;
  sync_point_client_->WaitNonThreadSafe(
      release_state.get(), release, task_runner_,
      base::Bind(&GpuCommandBufferStub::OnWaitFenceSyncCompleted,
                 this->AsWeakPtr(), namespace_id, command_buffer_id, release));

  if (!waiting_for_sync_point_)
    return true;

  scheduler_->SetScheduled(false);
  return false;
}

void GpuCommandBufferStub::OnWaitFenceSyncCompleted(
    gpu::CommandBufferNamespace namespace_id,
    uint64_t command_buffer_id,
    uint64_t release) {
  DCHECK(waiting_for_sync_point_);
  TRACE_EVENT_ASYNC_END1("gpu", "WaitFenceSync", this, "GpuCommandBufferStub",
                         this);
  PullTextureUpdates(namespace_id, command_buffer_id, release);
  waiting_for_sync_point_ = false;
  scheduler_->SetScheduled(true);
}

void GpuCommandBufferStub::OnCreateImage(
    const GpuCommandBufferMsg_CreateImage_Params& params) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnCreateImage");
  const int32_t id = params.id;
  const gfx::GpuMemoryBufferHandle& handle = params.gpu_memory_buffer;
  const gfx::Size& size = params.size;
  const gfx::BufferFormat& format = params.format;
  const uint32_t internalformat = params.internal_format;
  const uint64_t image_release_count = params.image_release_count;

  if (!decoder_)
    return;

  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  DCHECK(image_manager);
  if (image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image already exists with same ID.";
    return;
  }

  if (!gpu::ImageFactory::IsGpuMemoryBufferFormatSupported(
          format, decoder_->GetCapabilities())) {
    LOG(ERROR) << "Format is not supported.";
    return;
  }

  if (!gpu::ImageFactory::IsImageSizeValidForGpuMemoryBufferFormat(size,
                                                                   format)) {
    LOG(ERROR) << "Invalid image size for format.";
    return;
  }

  if (!gpu::ImageFactory::IsImageFormatCompatibleWithGpuMemoryBufferFormat(
          internalformat, format)) {
    LOG(ERROR) << "Incompatible image format.";
    return;
  }

  scoped_refptr<gl::GLImage> image = channel()->CreateImageForGpuMemoryBuffer(
      handle, size, format, internalformat);
  if (!image.get())
    return;

  image_manager->AddImage(image.get(), id);
  if (image_release_count) {
    sync_point_client_->ReleaseFenceSync(image_release_count);
  }
}

void GpuCommandBufferStub::OnDestroyImage(int32_t id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDestroyImage");

  if (!decoder_)
    return;

  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  DCHECK(image_manager);
  if (!image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image with ID doesn't exist.";
    return;
  }

  image_manager->RemoveImage(id);
}

void GpuCommandBufferStub::SendConsoleMessage(int32_t id,
                                              const std::string& message) {
  GPUCommandBufferConsoleMessage console_message;
  console_message.id = id;
  console_message.message = message;
  IPC::Message* msg = new GpuCommandBufferMsg_ConsoleMsg(
      route_id_, console_message);
  msg->set_unblock(true);
  Send(msg);
}

void GpuCommandBufferStub::SendCachedShader(
    const std::string& key, const std::string& shader) {
  channel_->CacheShader(key, shader);
}

void GpuCommandBufferStub::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void GpuCommandBufferStub::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

const gpu::gles2::FeatureInfo* GpuCommandBufferStub::GetFeatureInfo() const {
  return context_group_->feature_info();
}

gpu::gles2::MemoryTracker* GpuCommandBufferStub::GetMemoryTracker() const {
  return context_group_->memory_tracker();
}

bool GpuCommandBufferStub::CheckContextLost() {
  DCHECK(command_buffer_);
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  bool was_lost = state.error == gpu::error::kLostContext;

  if (was_lost) {
    bool was_lost_by_robustness =
        decoder_ && decoder_->WasContextLostByRobustnessExtension();

    // Work around issues with recovery by allowing a new GPU process to launch.
    if ((was_lost_by_robustness ||
         context_group_->feature_info()->workarounds().exit_on_context_lost) &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSingleProcess) &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kInProcessGPU)) {
      LOG(ERROR) << "Exiting GPU process because some drivers cannot recover"
                 << " from problems.";
#if defined(OS_WIN)
      base::win::SetShouldCrashOnProcessDetach(false);
#endif
      exit(0);
    }

    // Lose all other contexts if the reset was triggered by the robustness
    // extension instead of being synthetic.
    if (was_lost_by_robustness &&
        (gfx::GLContext::LosesAllContextsOnContextLost() ||
         use_virtualized_gl_context_)) {
      channel_->LoseAllContexts();
    }
  }

  CheckCompleteWaits();
  return was_lost;
}

void GpuCommandBufferStub::MarkContextLost() {
  if (!command_buffer_ ||
      command_buffer_->GetLastState().error == gpu::error::kLostContext)
    return;

  command_buffer_->SetContextLostReason(gpu::error::kUnknown);
  if (decoder_)
    decoder_->MarkContextLost(gpu::error::kUnknown);
  command_buffer_->SetParseError(gpu::error::kLostContext);
}

void GpuCommandBufferStub::SendSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result) {
  Send(new GpuCommandBufferMsg_SwapBuffersCompleted(route_id_, latency_info,
                                                    result));
}

void GpuCommandBufferStub::SendUpdateVSyncParameters(base::TimeTicks timebase,
                                                     base::TimeDelta interval) {
  Send(new GpuCommandBufferMsg_UpdateVSyncParameters(route_id_, timebase,
                                                     interval));
}

}  // namespace content
