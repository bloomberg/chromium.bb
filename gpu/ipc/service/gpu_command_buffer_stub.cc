// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_command_buffer_stub.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/crash_logging.h"
#include "base/hash.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/preemption_flag.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_memory_manager.h"
#include "gpu/ipc/service/gpu_memory_tracking.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_workarounds.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

#if defined(OS_ANDROID)
#include "gpu/ipc/service/stream_texture_android.h"
#endif

// Macro to reduce code duplication when logging memory in
// GpuCommandBufferMemoryTracker. This is needed as the UMA_HISTOGRAM_* macros
// require a unique call-site per histogram (you can't funnel multiple strings
// into the same call-site).
#define GPU_COMMAND_BUFFER_MEMORY_BLOCK(category)                          \
  do {                                                                     \
    uint64_t mb_used = tracking_group_->GetSize() / (1024 * 1024);         \
    switch (context_type_) {                                               \
      case gles2::CONTEXT_TYPE_WEBGL1:                                     \
      case gles2::CONTEXT_TYPE_WEBGL2:                                     \
        UMA_HISTOGRAM_MEMORY_LARGE_MB("GPU.ContextMemory.WebGL." category, \
                                      mb_used);                            \
        break;                                                             \
      case gles2::CONTEXT_TYPE_OPENGLES2:                                  \
      case gles2::CONTEXT_TYPE_OPENGLES3:                                  \
        UMA_HISTOGRAM_MEMORY_LARGE_MB("GPU.ContextMemory.GLES." category,  \
                                      mb_used);                            \
        break;                                                             \
    }                                                                      \
  } while (false)

namespace gpu {
struct WaitForCommandState {
  WaitForCommandState(int32_t start, int32_t end, IPC::Message* reply)
      : start(start), end(end), reply(reply) {}

  int32_t start;
  int32_t end;
  std::unique_ptr<IPC::Message> reply;
};

namespace {

// The GpuCommandBufferMemoryTracker class provides a bridge between the
// ContextGroup's memory type managers and the GpuMemoryManager class.
class GpuCommandBufferMemoryTracker : public gles2::MemoryTracker {
 public:
  explicit GpuCommandBufferMemoryTracker(
      GpuChannel* channel,
      uint64_t share_group_tracing_guid,
      gles2::ContextType context_type,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : tracking_group_(
            channel->gpu_channel_manager()
                ->gpu_memory_manager()
                ->CreateTrackingGroup(channel->GetClientPID(), this)),
        client_tracing_id_(channel->client_tracing_id()),
        client_id_(channel->client_id()),
        share_group_tracing_guid_(share_group_tracing_guid),
        context_type_(context_type),
        memory_pressure_listener_(new base::MemoryPressureListener(
            base::Bind(&GpuCommandBufferMemoryTracker::LogMemoryStatsPressure,
                       base::Unretained(this)))) {
    // Set up |memory_stats_timer_| to call LogMemoryPeriodic periodically
    // via the provided |task_runner|.
    memory_stats_timer_.SetTaskRunner(std::move(task_runner));
    memory_stats_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(30), this,
        &GpuCommandBufferMemoryTracker::LogMemoryStatsPeriodic);
  }

  void TrackMemoryAllocatedChange(size_t old_size, size_t new_size) override {
    tracking_group_->TrackMemoryAllocatedChange(old_size, new_size);
  }

  bool EnsureGPUMemoryAvailable(size_t size_needed) override {
    return tracking_group_->EnsureGPUMemoryAvailable(size_needed);
  }

  uint64_t ClientTracingId() const override { return client_tracing_id_; }
  int ClientId() const override { return client_id_; }
  uint64_t ShareGroupTracingGUID() const override {
    return share_group_tracing_guid_;
  }

 private:
  ~GpuCommandBufferMemoryTracker() override { LogMemoryStatsShutdown(); }

  void LogMemoryStatsPeriodic() { GPU_COMMAND_BUFFER_MEMORY_BLOCK("Periodic"); }
  void LogMemoryStatsShutdown() { GPU_COMMAND_BUFFER_MEMORY_BLOCK("Shutdown"); }
  void LogMemoryStatsPressure(
      base::MemoryPressureListener::MemoryPressureLevel pressure_level) {
    // Only log on CRITICAL memory pressure.
    if (pressure_level ==
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
      GPU_COMMAND_BUFFER_MEMORY_BLOCK("Pressure");
    }
  }

  std::unique_ptr<GpuMemoryTrackingGroup> tracking_group_;
  const uint64_t client_tracing_id_;
  const int client_id_;
  const uint64_t share_group_tracing_guid_;

  // Variables used in memory stat histogram logging.
  const gles2::ContextType context_type_;
  base::RepeatingTimer memory_stats_timer_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferMemoryTracker);
};

// FastSetActiveURL will shortcut the expensive call to SetActiveURL when the
// url_hash matches.
void FastSetActiveURL(const GURL& url, size_t url_hash, GpuChannel* channel) {
  // Leave the previously set URL in the empty case -- empty URLs are given by
  // BlinkPlatformImpl::createOffscreenGraphicsContext3DProvider. Hopefully the
  // onscreen context URL was set previously and will show up even when a crash
  // occurs during offscreen command processing.
  if (url.is_empty())
    return;
  static size_t g_last_url_hash = 0;
  if (url_hash != g_last_url_hash) {
    g_last_url_hash = url_hash;
    DCHECK(channel && channel->gpu_channel_manager() &&
           channel->gpu_channel_manager()->delegate());
    channel->gpu_channel_manager()->delegate()->SetActiveURL(url);
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
  static std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
  CreateForChannel(GpuChannel* channel);
  ~DevToolsChannelData() override {}

  void AppendAsTraceFormat(std::string* out) const override {
    std::string tmp;
    base::JSONWriter::Write(*value_, &tmp);
    *out += tmp;
  }

 private:
  explicit DevToolsChannelData(base::Value* value) : value_(value) {}
  std::unique_ptr<base::Value> value_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsChannelData);
};

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
DevToolsChannelData::CreateForChannel(GpuChannel* channel) {
  std::unique_ptr<base::DictionaryValue> res(new base::DictionaryValue);
  res->SetInteger("renderer_pid", channel->GetClientPID());
  res->SetDouble("used_bytes", channel->GetMemoryUsage());
  return base::WrapUnique(new DevToolsChannelData(res.release()));
}

}  // namespace

GpuCommandBufferStub::GpuCommandBufferStub(
    GpuChannel* channel,
    const GPUCreateCommandBufferConfig& init_params,
    CommandBufferId command_buffer_id,
    SequenceId sequence_id,
    int32_t stream_id,
    int32_t route_id)
    : channel_(channel),
      initialized_(false),
      surface_handle_(init_params.surface_handle),
      use_virtualized_gl_context_(false),
      command_buffer_id_(command_buffer_id),
      sequence_id_(sequence_id),
      stream_id_(stream_id),
      route_id_(route_id),
      last_flush_id_(0),
      waiting_for_sync_point_(false),
      previous_processed_num_(0),
      active_url_(init_params.active_url),
      active_url_hash_(base::Hash(active_url_.possibly_invalid_spec())),
      wait_set_get_buffer_count_(0) {}

GpuCommandBufferStub::~GpuCommandBufferStub() {
  Destroy();
}

GpuMemoryManager* GpuCommandBufferStub::GetMemoryManager() const {
  return channel()->gpu_channel_manager()->gpu_memory_manager();
}

bool GpuCommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "GPUTask",
               "data", DevToolsChannelData::CreateForChannel(channel()));
  FastSetActiveURL(active_url_, active_url_hash_, channel_);
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLContextIsVirtual,
                                use_virtualized_gl_context_ ? "1" : "0");
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
      message.type() != GpuCommandBufferMsg_WaitSyncToken::ID &&
      message.type() != GpuCommandBufferMsg_SignalSyncToken::ID &&
      message.type() != GpuCommandBufferMsg_SignalQuery::ID) {
    if (!MakeCurrent())
      return false;
    have_context = true;
  }

  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuCommandBufferStub, message)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetGetBuffer, OnSetGetBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_TakeFrontBuffer, OnTakeFrontBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ReturnFrontBuffer,
                        OnReturnFrontBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_WaitForTokenInRange,
                                    OnWaitForTokenInRange);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_WaitForGetOffsetInRange,
                                    OnWaitForGetOffsetInRange);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RegisterTransferBuffer,
                        OnRegisterTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyTransferBuffer,
                        OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_WaitSyncToken, OnWaitSyncToken)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalSyncToken, OnSignalSyncToken)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalQuery, OnSignalQuery)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateImage, OnCreateImage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyImage, OnDestroyImage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateStreamTexture,
                        OnCreateStreamTexture)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  CheckCompleteWaits();

  // Ensure that any delayed work that was created will be handled.
  if (have_context) {
    if (decoder_)
      decoder_->ProcessPendingQueries(false);
    ScheduleDelayedWork(
        base::TimeDelta::FromMilliseconds(kHandleMoreWorkPeriodMs));
  }

  return handled;
}

bool GpuCommandBufferStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}

#if defined(OS_WIN)
void GpuCommandBufferStub::DidCreateAcceleratedSurfaceChildWindow(
    SurfaceHandle parent_window,
    SurfaceHandle child_window) {
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->delegate()->SendAcceleratedSurfaceCreatedChildWindow(
      parent_window, child_window);
}
#endif

void GpuCommandBufferStub::DidSwapBuffersComplete(
    SwapBuffersCompleteParams params) {
  GpuCommandBufferMsg_SwapBuffersCompleted_Params send_params;
#if defined(OS_MACOSX)
  send_params.ca_context_id = params.ca_context_id;
  send_params.fullscreen_low_power_ca_context_valid =
      params.fullscreen_low_power_ca_context_valid;
  send_params.fullscreen_low_power_ca_context_id =
      params.fullscreen_low_power_ca_context_id;
  send_params.io_surface = params.io_surface;
  send_params.pixel_size = params.pixel_size;
  send_params.scale_factor = params.scale_factor;
  send_params.in_use_responses = params.in_use_responses;
#endif
  send_params.latency_info = std::move(params.latency_info);
  send_params.result = params.result;
  Send(new GpuCommandBufferMsg_SwapBuffersCompleted(route_id_, send_params));
}

const gles2::FeatureInfo* GpuCommandBufferStub::GetFeatureInfo() const {
  return context_group_->feature_info();
}

const GpuPreferences& GpuCommandBufferStub::GetGpuPreferences() const {
  return context_group_->gpu_preferences();
}

void GpuCommandBufferStub::SetLatencyInfoCallback(
    const LatencyInfoCallback& callback) {
  latency_info_callback_ = callback;
}

void GpuCommandBufferStub::UpdateVSyncParameters(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  Send(new GpuCommandBufferMsg_UpdateVSyncParameters(route_id_, timebase,
                                                     interval));
}

void GpuCommandBufferStub::AddFilter(IPC::MessageFilter* message_filter) {
  return channel_->AddFilter(message_filter);
}

int32_t GpuCommandBufferStub::GetRouteID() const {
  return route_id_;
}

bool GpuCommandBufferStub::IsScheduled() {
  return (!command_buffer_.get() || command_buffer_->scheduled());
}

void GpuCommandBufferStub::PollWork() {
  // Post another delayed task if we have not yet reached the time at which
  // we should process delayed work.
  base::TimeTicks current_time = base::TimeTicks::Now();
  DCHECK(!process_delayed_work_time_.is_null());
  if (process_delayed_work_time_ > current_time) {
    channel_->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&GpuCommandBufferStub::PollWork, AsWeakPtr()),
        process_delayed_work_time_ - current_time);
    return;
  }
  process_delayed_work_time_ = base::TimeTicks();

  PerformWork();
}

void GpuCommandBufferStub::PerformWork() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::PerformWork");
  FastSetActiveURL(active_url_, active_url_hash_, channel_);
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLContextIsVirtual,
                                use_virtualized_gl_context_ ? "1" : "0");
  if (decoder_.get() && !MakeCurrent())
    return;

  if (decoder_) {
    uint32_t current_unprocessed_num =
        channel()->sync_point_manager()->GetUnprocessedOrderNum();
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
      decoder_->PerformIdleWork();
    }

    decoder_->ProcessPendingQueries(false);
    decoder_->PerformPollingWork();
  }

  ScheduleDelayedWork(
      base::TimeDelta::FromMilliseconds(kHandleMoreWorkPeriodBusyMs));
}

bool GpuCommandBufferStub::HasUnprocessedCommands() {
  if (command_buffer_) {
    CommandBuffer::State state = command_buffer_->GetState();
    return command_buffer_->put_offset() != state.get_offset &&
           !error::IsError(state.error);
  }
  return false;
}

void GpuCommandBufferStub::ScheduleDelayedWork(base::TimeDelta delay) {
  bool has_more_work = decoder_.get() && (decoder_->HasPendingQueries() ||
                                          decoder_->HasMoreIdleWork() ||
                                          decoder_->HasPollingWork());
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
      channel()->sync_point_manager()->GetProcessedOrderNum();
  if (last_idle_time_.is_null())
    last_idle_time_ = current_time;

  // IsScheduled() returns true after passing all unschedule fences
  // and this is when we can start performing idle work. Idle work
  // is done synchronously so we can set delay to 0 and instead poll
  // for more work at the rate idle work is performed. This also ensures
  // that idle work is done as efficiently as possible without any
  // unnecessary delays.
  if (command_buffer_->scheduled() && decoder_->HasMoreIdleWork()) {
    delay = base::TimeDelta();
  }

  process_delayed_work_time_ = current_time + delay;
  channel_->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&GpuCommandBufferStub::PollWork, AsWeakPtr()),
      delay);
}

bool GpuCommandBufferStub::MakeCurrent() {
  if (decoder_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  command_buffer_->SetParseError(error::kLostContext);
  CheckContextLost();
  return false;
}

void GpuCommandBufferStub::Destroy() {
  FastSetActiveURL(active_url_, active_url_hash_, channel_);
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLContextIsVirtual,
                                use_virtualized_gl_context_ ? "1" : "0");
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
    // If we are currently shutting down the GPU process to help with recovery
    // (exit_on_context_lost workaround), then don't tell the browser about
    // offscreen context destruction here since it's not client-invoked, and
    // might bypass the 3D API blocking logic.
    if ((surface_handle_ == gpu::kNullSurfaceHandle) &&
        !active_url_.is_empty() &&
        !gpu_channel_manager->is_exiting_for_lost_context()) {
      gpu_channel_manager->delegate()->DidDestroyOffscreenContext(active_url_);
    }
  }

  if (sync_point_client_state_) {
    sync_point_client_state_->Destroy();
    sync_point_client_state_ = nullptr;
  }

  bool have_context = false;
  if (decoder_ && decoder_->GetGLContext()) {
    // Try to make the context current regardless of whether it was lost, so we
    // don't leak resources.
    have_context = decoder_->GetGLContext()->MakeCurrent(surface_.get());
  }
  for (auto& observer : destruction_observers_)
    observer.OnWillDestroyStub();

  share_group_ = nullptr;

  // Remove this after crbug.com/248395 is sorted out.
  // Destroy the surface before the context, some surface destructors make GL
  // calls.
  surface_ = nullptr;

  if (decoder_) {
    decoder_->Destroy(have_context);
    decoder_.reset();
  }

  command_buffer_.reset();
}

gpu::ContextResult GpuCommandBufferStub::Initialize(
    GpuCommandBufferStub* share_command_buffer_stub,
    const GPUCreateCommandBufferConfig& init_params,
    std::unique_ptr<base::SharedMemory> shared_state_shm) {
#if defined(OS_FUCHSIA)
  // TODO(crbug.com/707031): Implement this.
  NOTIMPLEMENTED();
  return gpu::ContextResult::kFatalFailure;
#else
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::Initialize");
  FastSetActiveURL(active_url_, active_url_hash_, channel_);

  GpuChannelManager* manager = channel_->gpu_channel_manager();
  DCHECK(manager);

  if (share_command_buffer_stub) {
    context_group_ = share_command_buffer_stub->context_group_;
    DCHECK(context_group_->bind_generates_resource() ==
           init_params.attribs.bind_generates_resource);
  } else {
    scoped_refptr<gles2::FeatureInfo> feature_info =
        new gles2::FeatureInfo(manager->gpu_driver_bug_workarounds());
    gpu::GpuMemoryBufferFactory* gmb_factory =
        manager->gpu_memory_buffer_factory();
    context_group_ = new gles2::ContextGroup(
        manager->gpu_preferences(), gles2::PassthroughCommandDecoderSupported(),
        manager->mailbox_manager(),
        new GpuCommandBufferMemoryTracker(
            channel_, command_buffer_id_.GetUnsafeValue(),
            init_params.attribs.context_type, channel_->task_runner()),
        manager->shader_translator_cache(),
        manager->framebuffer_completeness_cache(), feature_info,
        init_params.attribs.bind_generates_resource, channel_->image_manager(),
        gmb_factory ? gmb_factory->AsImageFactory() : nullptr,
        manager->watchdog() /* progress_reporter */,
        manager->gpu_feature_info(), manager->discardable_manager());
  }

#if defined(OS_MACOSX)
  // Virtualize PreferIntegratedGpu contexts by default on OS X to prevent
  // performance regressions when enabling FCM.
  // http://crbug.com/180463
  if (init_params.attribs.gpu_preference == gl::PreferIntegratedGpu)
    use_virtualized_gl_context_ = true;
#endif

  use_virtualized_gl_context_ |=
      context_group_->feature_info()->workarounds().use_virtualized_gl_contexts;

  // MailboxManagerSync synchronization correctness currently depends on having
  // only a single context. See crbug.com/510243 for details.
  use_virtualized_gl_context_ |= manager->mailbox_manager()->UsesSync();

  bool offscreen = (surface_handle_ == kNullSurfaceHandle);
  gl::GLSurface* default_surface = manager->GetDefaultOffscreenSurface();
  if (!default_surface) {
    DLOG(ERROR) << "Failed to create default offscreen surface.";
    return gpu::ContextResult::kFatalFailure;
  }
  // On low-spec Android devices, the default offscreen surface is
  // RGB565, but WebGL rendering contexts still ask for RGBA8888 mode.
  // That combination works for offscreen rendering, we can still use
  // a virtualized context with the RGB565 backing surface since we're
  // not drawing to that. Explicitly set that as the desired surface
  // format to ensure it's treated as compatible where applicable.
  gl::GLSurfaceFormat surface_format =
      offscreen ? default_surface->GetFormat() : gl::GLSurfaceFormat();
#if defined(OS_ANDROID)
  if (init_params.attribs.red_size <= 5 &&
      init_params.attribs.green_size <= 6 &&
      init_params.attribs.blue_size <= 5 &&
      init_params.attribs.alpha_size == 0) {
    // We hit this code path when creating the onscreen render context
    // used for compositing on low-end Android devices.
    //
    // TODO(klausw): explicitly copy rgba sizes? Currently the formats
    // supported are only RGB565 and default (RGBA8888).
    surface_format.SetRGB565();
    DVLOG(1) << __FUNCTION__ << ": Choosing RGB565 mode.";
  }

  // We can only use virtualized contexts for onscreen command buffers if their
  // config is compatible with the offscreen ones - otherwise MakeCurrent fails.
  // Example use case is a client requesting an onscreen RGBA8888 buffer for
  // fullscreen video on a low-spec device with RGB565 default format.
  if (!surface_format.IsCompatible(default_surface->GetFormat()) && !offscreen)
    use_virtualized_gl_context_ = false;
#endif

  command_buffer_ = std::make_unique<CommandBufferService>(
      this, context_group_->transfer_buffer_manager());
  decoder_.reset(gles2::GLES2Decoder::Create(
      this, command_buffer_.get(), manager->outputter(), context_group_.get()));

  sync_point_client_state_ =
      channel_->sync_point_manager()->CreateSyncPointClientState(
          CommandBufferNamespace::GPU_IO, command_buffer_id_, sequence_id_);

  if (offscreen) {
    // Do we want to create an offscreen rendering context suitable
    // for directly drawing to a separately supplied surface? In that
    // case, we must ensure that the surface used for context creation
    // is compatible with the requested attributes. This is explicitly
    // opt-in since some context such as for NaCl request custom
    // attributes but don't expect to get their own surface, and not
    // all surface factories support custom formats.
    if (init_params.attribs.own_offscreen_surface) {
      if (init_params.attribs.depth_size > 0) {
        surface_format.SetDepthBits(init_params.attribs.depth_size);
      }
      if (init_params.attribs.samples > 0) {
        surface_format.SetSamples(init_params.attribs.samples);
      }
      if (init_params.attribs.stencil_size > 0) {
        surface_format.SetStencilBits(init_params.attribs.stencil_size);
      }
      // Currently, we can't separately control alpha channel for surfaces,
      // it's generally enabled by default except for RGB565 and (on desktop)
      // smaller-than-32bit formats.
      //
      // TODO(klausw): use init_params.attribs.alpha_size here if possible.
    }
    if (!surface_format.IsCompatible(default_surface->GetFormat())) {
      DVLOG(1) << __FUNCTION__ << ": Hit the OwnOffscreenSurface path";
      use_virtualized_gl_context_ = false;
      surface_ = gl::init::CreateOffscreenGLSurfaceWithFormat(gfx::Size(),
                                                              surface_format);
      if (!surface_) {
        DLOG(ERROR) << "Failed to create surface.";
        return gpu::ContextResult::kFatalFailure;
      }
    } else {
      surface_ = default_surface;
    }
  } else {
    switch (init_params.attribs.color_space) {
      case gles2::COLOR_SPACE_UNSPECIFIED:
        surface_format.SetColorSpace(
            gl::GLSurfaceFormat::COLOR_SPACE_UNSPECIFIED);
        break;
      case gles2::COLOR_SPACE_SRGB:
        surface_format.SetColorSpace(gl::GLSurfaceFormat::COLOR_SPACE_SRGB);
        break;
      case gles2::COLOR_SPACE_DISPLAY_P3:
        surface_format.SetColorSpace(
            gl::GLSurfaceFormat::COLOR_SPACE_DISPLAY_P3);
        break;
    }
    surface_ = ImageTransportSurface::CreateNativeSurface(
        AsWeakPtr(), surface_handle_, surface_format);
    if (!surface_ || !surface_->Initialize(surface_format)) {
      surface_ = nullptr;
      DLOG(ERROR) << "Failed to create surface.";
      return gpu::ContextResult::kFatalFailure;
    }
  }

  if (context_group_->use_passthrough_cmd_decoder()) {
    // When using the passthrough command decoder, only share with other
    // contexts in the explicitly requested share group
    if (share_command_buffer_stub) {
      share_group_ = share_command_buffer_stub->share_group_;
    } else {
      share_group_ = new gl::GLShareGroup();
    }
  } else {
    // When using the validating command decoder, always use the global share
    // group
    share_group_ = channel_->share_group();
  }

  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLContextIsVirtual,
                                use_virtualized_gl_context_ ? "1" : "0");

  scoped_refptr<gl::GLContext> context;
  if (use_virtualized_gl_context_ && share_group_) {
    context = share_group_->GetSharedContext(surface_.get());
    if (!context) {
      context = gl::init::CreateGLContext(
          share_group_.get(), surface_.get(),
          GenerateGLContextAttribs(init_params.attribs, context_group_.get()));
      if (!context) {
        DLOG(ERROR) << "Failed to create shared context for virtualization.";
        // TODO(piman): This might not be fatal, we could recurse into
        // CreateGLContext to get more info, tho it should be exceedingly
        // rare and may not be recoverable anyway.
        return gpu::ContextResult::kFatalFailure;
      }
      // Ensure that context creation did not lose track of the intended share
      // group.
      DCHECK(context->share_group() == share_group_.get());
      share_group_->SetSharedContext(surface_.get(), context.get());

      // This needs to be called against the real shared context, not the
      // virtual context created below.
      manager->gpu_feature_info().ApplyToGLContext(context.get());
    }
    // This should be either:
    // (1) a non-virtual GL context, or
    // (2) a mock/stub context.
    DCHECK(context->GetHandle() ||
           gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
           gl::GetGLImplementation() == gl::kGLImplementationStubGL);
    context = base::MakeRefCounted<GLContextVirtual>(
        share_group_.get(), context.get(), decoder_->AsWeakPtr());
    if (!context->Initialize(surface_.get(),
                             GenerateGLContextAttribs(init_params.attribs,
                                                      context_group_.get()))) {
      // The real context created above for the default offscreen surface
      // might not be compatible with this surface.
      context = nullptr;
      DLOG(ERROR) << "Failed to initialize virtual GL context.";
      // TODO(piman): This might not be fatal, we could recurse into
      // CreateGLContext to get more info, tho it should be exceedingly
      // rare and may not be recoverable anyway.
      return gpu::ContextResult::kFatalFailure;
    }
  } else {
    context = gl::init::CreateGLContext(
        share_group_.get(), surface_.get(),
        GenerateGLContextAttribs(init_params.attribs, context_group_.get()));
    if (!context) {
      DLOG(ERROR) << "Failed to create context.";
      // TODO(piman): This might not be fatal, we could recurse into
      // CreateGLContext to get more info, tho it should be exceedingly
      // rare and may not be recoverable anyway.
      return gpu::ContextResult::kFatalFailure;
    }

    manager->gpu_feature_info().ApplyToGLContext(context.get());
  }

  if (!context->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Failed to make context current.";
    return gpu::ContextResult::kTransientFailure;
  }

  if (!context->GetGLStateRestorer()) {
    context->SetGLStateRestorer(new GLStateRestorerImpl(decoder_->AsWeakPtr()));
  }

  if (!context_group_->has_program_cache() &&
      !context_group_->feature_info()->workarounds().disable_program_cache) {
    context_group_->set_program_cache(manager->program_cache());
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  auto result = decoder_->Initialize(surface_, context, offscreen,
                                     gpu::gles2::DisallowedFeatures(),
                                     init_params.attribs);
  if (result != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize decoder.";
    return result;
  }

  if (manager->gpu_preferences().enable_gpu_service_logging) {
    decoder_->set_log_commands(true);
  }

  const size_t kSharedStateSize = sizeof(CommandBufferSharedState);
  if (!shared_state_shm->Map(kSharedStateSize)) {
    DLOG(ERROR) << "Failed to map shared state buffer.";
    return gpu::ContextResult::kFatalFailure;
  }
  command_buffer_->SetSharedStateBuffer(MakeBackingFromSharedMemory(
      std::move(shared_state_shm), kSharedStateSize));

  if (offscreen && !active_url_.is_empty())
    manager->delegate()->DidCreateOffscreenContext(active_url_);

  if (use_virtualized_gl_context_) {
    // If virtualized GL contexts are in use, then real GL context state
    // is in an indeterminate state, since the GLStateRestorer was not
    // initialized at the time the GLContextVirtual was made current. In
    // the case that this command decoder is the next one to be
    // processed, force a "full virtual" MakeCurrent to be performed.
    // Note that GpuChannel's initialization of the gpu::Capabilities
    // expects the context to be left current.
    context->ForceReleaseVirtuallyCurrent();
    if (!context->MakeCurrent(surface_.get())) {
      LOG(ERROR) << "Failed to make context current after initialization.";
      return gpu::ContextResult::kTransientFailure;
    }
  }

  initialized_ = true;
  return gpu::ContextResult::kSuccess;
#endif  // defined(OS_FUCHSIA)
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

void GpuCommandBufferStub::OnSetGetBuffer(int32_t shm_id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnSetGetBuffer");
  if (command_buffer_)
    command_buffer_->SetGetBuffer(shm_id);
}

void GpuCommandBufferStub::OnTakeFrontBuffer(const Mailbox& mailbox) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnTakeFrontBuffer");
  if (!decoder_) {
    LOG(ERROR) << "Can't take front buffer before initialization.";
    return;
  }

  decoder_->TakeFrontBuffer(mailbox);
}

void GpuCommandBufferStub::OnReturnFrontBuffer(const Mailbox& mailbox,
                                               bool is_lost) {
  decoder_->ReturnFrontBuffer(mailbox, is_lost);
}

CommandBufferServiceClient::CommandBatchProcessedResult
GpuCommandBufferStub::OnCommandBatchProcessed() {
  GpuWatchdogThread* watchdog = channel_->gpu_channel_manager()->watchdog();
  if (watchdog)
    watchdog->CheckArmed();
  bool pause = false;
  if (channel_->scheduler()) {
    pause = channel_->scheduler()->ShouldYield(sequence_id_);
  } else if (channel_->preempted_flag()) {
    pause = channel_->preempted_flag()->IsSet();
  }
  return pause ? kPauseExecution : kContinueExecution;
}

void GpuCommandBufferStub::OnParseError() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnParseError");
  DCHECK(command_buffer_.get());
  CommandBuffer::State state = command_buffer_->GetState();
  IPC::Message* msg = new GpuCommandBufferMsg_Destroyed(
      route_id_, state.context_lost_reason, state.error);
  msg->set_unblock(true);
  Send(msg);

  // Tell the browser about this context loss as well, so it can
  // determine whether client APIs like WebGL need to be immediately
  // blocked from automatically running.
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->delegate()->DidLoseContext(
      (surface_handle_ == kNullSurfaceHandle), state.context_lost_reason,
      active_url_);

  CheckContextLost();
}

void GpuCommandBufferStub::OnWaitForTokenInRange(int32_t start,
                                                 int32_t end,
                                                 IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnWaitForTokenInRange");
  DCHECK(command_buffer_.get());
  CheckContextLost();
  if (wait_for_token_)
    LOG(ERROR) << "Got WaitForToken command while currently waiting for token.";
  if (channel_->scheduler()) {
    channel_->scheduler()->RaisePriorityForClientWait(sequence_id_,
                                                      command_buffer_id_);
  }
  wait_for_token_ =
      std::make_unique<WaitForCommandState>(start, end, reply_message);
  CheckCompleteWaits();
}

void GpuCommandBufferStub::OnWaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
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
  if (channel_->scheduler()) {
    channel_->scheduler()->RaisePriorityForClientWait(sequence_id_,
                                                      command_buffer_id_);
  }
  wait_for_get_offset_ =
      std::make_unique<WaitForCommandState>(start, end, reply_message);
  wait_set_get_buffer_count_ = set_get_buffer_count;
  CheckCompleteWaits();
}

void GpuCommandBufferStub::CheckCompleteWaits() {
  bool has_wait = wait_for_token_ || wait_for_get_offset_;
  if (has_wait) {
    CommandBuffer::State state = command_buffer_->GetState();
    if (wait_for_token_ &&
        (CommandBuffer::InRange(wait_for_token_->start, wait_for_token_->end,
                                state.token) ||
         state.error != error::kNoError)) {
      ReportState();
      GpuCommandBufferMsg_WaitForTokenInRange::WriteReplyParams(
          wait_for_token_->reply.get(), state);
      Send(wait_for_token_->reply.release());
      wait_for_token_.reset();
    }
    if (wait_for_get_offset_ &&
        (((wait_set_get_buffer_count_ == state.set_get_buffer_count) &&
          CommandBuffer::InRange(wait_for_get_offset_->start,
                                 wait_for_get_offset_->end,
                                 state.get_offset)) ||
         state.error != error::kNoError)) {
      ReportState();
      GpuCommandBufferMsg_WaitForGetOffsetInRange::WriteReplyParams(
          wait_for_get_offset_->reply.get(), state);
      Send(wait_for_get_offset_->reply.release());
      wait_for_get_offset_.reset();
    }
  }
  if (channel_->scheduler() && has_wait &&
      !(wait_for_token_ || wait_for_get_offset_)) {
    channel_->scheduler()->ResetPriorityForClientWait(sequence_id_,
                                                      command_buffer_id_);
  }
}

void GpuCommandBufferStub::OnAsyncFlush(
    int32_t put_offset,
    uint32_t flush_id,
    const std::vector<ui::LatencyInfo>& latency_info) {
  TRACE_EVENT1(
      "gpu", "GpuCommandBufferStub::OnAsyncFlush", "put_offset", put_offset);
  DCHECK(command_buffer_);

  // We received this message out-of-order. This should not happen but is here
  // to catch regressions. Ignore the message.
  DVLOG_IF(0, flush_id - last_flush_id_ >= 0x8000000U)
      << "Received a Flush message out-of-order";

  if (flush_id > last_flush_id_ &&
      ui::LatencyInfo::Verify(latency_info,
                              "GpuCommandBufferStub::OnAsyncFlush") &&
      !latency_info_callback_.is_null()) {
    latency_info_callback_.Run(latency_info);
  }

  last_flush_id_ = flush_id;
  CommandBuffer::State pre_state = command_buffer_->GetState();
  FastSetActiveURL(active_url_, active_url_hash_, channel_);
  command_buffer_->Flush(put_offset, decoder_.get());
  CommandBuffer::State post_state = command_buffer_->GetState();

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
  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(transfer_buffer, false));
  if (!shared_memory->Map(size)) {
    DVLOG(0) << "Failed to map shared memory.";
    return;
  }

  if (command_buffer_) {
    command_buffer_->RegisterTransferBuffer(
        id, MakeBackingFromSharedMemory(std::move(shared_memory), size));
  }
}

void GpuCommandBufferStub::OnDestroyTransferBuffer(int32_t id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDestroyTransferBuffer");

  if (command_buffer_)
    command_buffer_->DestroyTransferBuffer(id);
}

void GpuCommandBufferStub::ReportState() {
  command_buffer_->UpdateState();
}

void GpuCommandBufferStub::OnSignalSyncToken(const SyncToken& sync_token,
                                             uint32_t id) {
  if (!sync_point_client_state_->WaitNonThreadSafe(
          sync_token, channel_->task_runner(),
          base::Bind(&GpuCommandBufferStub::OnSignalAck, this->AsWeakPtr(),
                     id))) {
    OnSignalAck(id);
  }
}

void GpuCommandBufferStub::OnSignalAck(uint32_t id) {
  CommandBuffer::State state = command_buffer_->GetState();
  ReportState();
  Send(new GpuCommandBufferMsg_SignalAck(route_id_, id, state));
}

void GpuCommandBufferStub::OnSignalQuery(uint32_t query_id, uint32_t id) {
  if (decoder_) {
    gles2::QueryManager* query_manager = decoder_->GetQueryManager();
    if (query_manager) {
      gles2::QueryManager::Query* query = query_manager->GetQuery(query_id);
      if (query) {
        query->AddCallback(base::Bind(&GpuCommandBufferStub::OnSignalAck,
                                      this->AsWeakPtr(), id));
        return;
      }
    }
  }
  // Something went wrong, run callback immediately.
  OnSignalAck(id);
}

void GpuCommandBufferStub::OnFenceSyncRelease(uint64_t release) {
  SyncToken sync_token(CommandBufferNamespace::GPU_IO, 0, command_buffer_id_,
                       release);
  gles2::MailboxManager* mailbox_manager = context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync() && MakeCurrent())
    mailbox_manager->PushTextureUpdates(sync_token);

  command_buffer_->SetReleaseCount(release);
  sync_point_client_state_->ReleaseFenceSync(release);
}

void GpuCommandBufferStub::OnDescheduleUntilFinished() {
  DCHECK(command_buffer_->scheduled());
  DCHECK(decoder_->HasPollingWork());

  command_buffer_->SetScheduled(false);
  channel_->OnCommandBufferDescheduled(this);
}

void GpuCommandBufferStub::OnRescheduleAfterFinished() {
  DCHECK(!command_buffer_->scheduled());

  command_buffer_->SetScheduled(true);
  channel_->OnCommandBufferScheduled(this);
}

bool GpuCommandBufferStub::OnWaitSyncToken(const SyncToken& sync_token) {
  DCHECK(!waiting_for_sync_point_);
  DCHECK(command_buffer_->scheduled());
  TRACE_EVENT_ASYNC_BEGIN1("gpu", "WaitSyncToken", this, "GpuCommandBufferStub",
                           this);

  waiting_for_sync_point_ = sync_point_client_state_->WaitNonThreadSafe(
      sync_token, channel_->task_runner(),
      base::Bind(&GpuCommandBufferStub::OnWaitSyncTokenCompleted, AsWeakPtr(),
                 sync_token));

  if (waiting_for_sync_point_) {
    command_buffer_->SetScheduled(false);
    channel_->OnCommandBufferDescheduled(this);
    return true;
  }

  gles2::MailboxManager* mailbox_manager = context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync() && MakeCurrent())
    mailbox_manager->PullTextureUpdates(sync_token);
  return false;
}

void GpuCommandBufferStub::OnWaitSyncTokenCompleted(
    const SyncToken& sync_token) {
  DCHECK(waiting_for_sync_point_);
  TRACE_EVENT_ASYNC_END1("gpu", "WaitSyncTokenCompleted", this,
                         "GpuCommandBufferStub", this);
  // Don't call PullTextureUpdates here because we can't MakeCurrent if we're
  // executing commands on another context. The WaitSyncToken command will run
  // again and call PullTextureUpdates once this command buffer gets scheduled.
  waiting_for_sync_point_ = false;
  command_buffer_->SetScheduled(true);
  channel_->OnCommandBufferScheduled(this);
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

  gles2::ImageManager* image_manager = channel_->image_manager();
  DCHECK(image_manager);
  if (image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image already exists with same ID.";
    return;
  }

  if (!gpu::IsImageFromGpuMemoryBufferFormatSupported(
          format, decoder_->GetCapabilities())) {
    LOG(ERROR) << "Format is not supported.";
    return;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, format)) {
    LOG(ERROR) << "Invalid image size for format.";
    return;
  }

  if (!gpu::IsImageFormatCompatibleWithGpuMemoryBufferFormat(internalformat,
                                                             format)) {
    LOG(ERROR) << "Incompatible image format.";
    return;
  }

  scoped_refptr<gl::GLImage> image = channel()->CreateImageForGpuMemoryBuffer(
      handle, size, format, internalformat, surface_handle_);
  if (!image.get())
    return;

  image_manager->AddImage(image.get(), id);
  if (image_release_count)
    sync_point_client_state_->ReleaseFenceSync(image_release_count);
}

void GpuCommandBufferStub::OnDestroyImage(int32_t id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDestroyImage");

  gles2::ImageManager* image_manager = channel_->image_manager();
  DCHECK(image_manager);
  if (!image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image with ID doesn't exist.";
    return;
  }

  image_manager->RemoveImage(id);
}

void GpuCommandBufferStub::OnConsoleMessage(int32_t id,
                                            const std::string& message) {
  GPUCommandBufferConsoleMessage console_message;
  console_message.id = id;
  console_message.message = message;
  IPC::Message* msg =
      new GpuCommandBufferMsg_ConsoleMsg(route_id_, console_message);
  msg->set_unblock(true);
  Send(msg);
}

void GpuCommandBufferStub::CacheShader(const std::string& key,
                                       const std::string& shader) {
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

gles2::MemoryTracker* GpuCommandBufferStub::GetMemoryTracker() const {
  return context_group_->memory_tracker();
}

bool GpuCommandBufferStub::CheckContextLost() {
  DCHECK(command_buffer_);
  CommandBuffer::State state = command_buffer_->GetState();
  bool was_lost = state.error == error::kLostContext;

  if (was_lost) {
    bool was_lost_by_robustness =
        decoder_ && decoder_->WasContextLostByRobustnessExtension();

    // Work around issues with recovery by allowing a new GPU process to launch.
    if ((was_lost_by_robustness ||
         context_group_->feature_info()->workarounds().exit_on_context_lost)) {
      channel_->gpu_channel_manager()->MaybeExitOnContextLost();
    }

    // Lose all other contexts if the reset was triggered by the robustness
    // extension instead of being synthetic.
    if (was_lost_by_robustness &&
        (gl::GLContext::LosesAllContextsOnContextLost() ||
         use_virtualized_gl_context_)) {
      channel_->LoseAllContexts();
    }
  }

  CheckCompleteWaits();
  return was_lost;
}

void GpuCommandBufferStub::MarkContextLost() {
  if (!command_buffer_ ||
      command_buffer_->GetState().error == error::kLostContext)
    return;

  command_buffer_->SetContextLostReason(error::kUnknown);
  if (decoder_)
    decoder_->MarkContextLost(error::kUnknown);
  command_buffer_->SetParseError(error::kLostContext);
}

}  // namespace gpu
