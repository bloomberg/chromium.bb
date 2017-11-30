// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/in_process_command_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/process/process_handle.h"
#endif

#if defined(OS_MACOSX)
#include "gpu/ipc/client/gpu_process_hosted_ca_layer_tree_params.h"
#endif

namespace gpu {

namespace {

base::AtomicSequenceNumber g_next_command_buffer_id;
base::AtomicSequenceNumber g_next_image_id;

template <typename T>
static void RunTaskWithResult(base::Callback<T(void)> task,
                              T* result,
                              base::WaitableEvent* completion) {
  *result = task.Run();
  completion->Signal();
}

class GpuInProcessThreadHolder : public base::Thread {
 public:
  GpuInProcessThreadHolder()
      : base::Thread("GpuThread"), sync_point_manager_(new SyncPointManager()) {
    Start();
  }

  ~GpuInProcessThreadHolder() override { Stop(); }

  void SetGpuFeatureInfo(const GpuFeatureInfo& gpu_feature_info) {
    DCHECK(!gpu_thread_service_.get());
    gpu_feature_info_ = gpu_feature_info;
  }

  const scoped_refptr<InProcessCommandBuffer::Service>& GetGpuThreadService() {
    if (!gpu_thread_service_) {
      gpu_thread_service_ = new GpuInProcessThreadService(
          task_runner(), sync_point_manager_.get(), nullptr, nullptr,
          gpu_feature_info_);
    }
    return gpu_thread_service_;
  }

 private:
  std::unique_ptr<SyncPointManager> sync_point_manager_;
  scoped_refptr<InProcessCommandBuffer::Service> gpu_thread_service_;
  GpuFeatureInfo gpu_feature_info_;
};

base::LazyInstance<GpuInProcessThreadHolder>::DestructorAtExit
    g_default_service = LAZY_INSTANCE_INITIALIZER;

class ScopedEvent {
 public:
  explicit ScopedEvent(base::WaitableEvent* event) : event_(event) {}
  ~ScopedEvent() { event_->Signal(); }

 private:
  base::WaitableEvent* event_;
};

scoped_refptr<InProcessCommandBuffer::Service> GetInitialService(
    const scoped_refptr<InProcessCommandBuffer::Service>& service) {
  if (service)
    return service;

  // Call base::ThreadTaskRunnerHandle::IsSet() to ensure that it is
  // instantiated before we create the GPU thread, otherwise shutdown order will
  // delete the ThreadTaskRunnerHandle before the GPU thread's message loop,
  // and when the message loop is shutdown, it will recreate
  // ThreadTaskRunnerHandle, which will re-add a new task to the, AtExitManager,
  // which causes a deadlock because it's already locked.
  base::ThreadTaskRunnerHandle::IsSet();
  return g_default_service.Get().GetGpuThreadService();
}

}  // anonyous namespace

InProcessCommandBuffer::Service::Service(
    const GpuPreferences& gpu_preferences,
    gles2::MailboxManager* mailbox_manager,
    scoped_refptr<gl::GLShareGroup> share_group,
    const GpuFeatureInfo& gpu_feature_info)
    : gpu_preferences_(gpu_preferences),
      gpu_feature_info_(gpu_feature_info),
      mailbox_manager_(mailbox_manager),
      share_group_(share_group),
      shader_translator_cache_(gpu_preferences_) {
  if (!mailbox_manager_) {
    // TODO(piman): have embedders own the mailbox manager.
    owned_mailbox_manager_ = gles2::MailboxManager::Create(gpu_preferences_);
    mailbox_manager_ = owned_mailbox_manager_.get();
  }
}

InProcessCommandBuffer::Service::~Service() {}

const GpuPreferences& InProcessCommandBuffer::Service::gpu_preferences() {
  return gpu_preferences_;
}

scoped_refptr<gl::GLShareGroup> InProcessCommandBuffer::Service::share_group() {
  if (!share_group_.get())
    share_group_ = new gl::GLShareGroup();
  return share_group_;
}

gles2::Outputter* InProcessCommandBuffer::Service::outputter() {
  if (!outputter_)
    outputter_.reset(new gles2::TraceOutputter("InProcessCommandBuffer Trace"));
  return outputter_.get();
}

gles2::ProgramCache* InProcessCommandBuffer::Service::program_cache() {
  if (!program_cache_.get() &&
      (gl::g_current_gl_driver->ext.b_GL_ARB_get_program_binary ||
       gl::g_current_gl_driver->ext.b_GL_OES_get_program_binary) &&
      !gpu_preferences().disable_gpu_program_cache) {
    bool disable_disk_cache =
        gpu_preferences_.disable_gpu_shader_disk_cache ||
        gpu_feature_info_.IsWorkaroundEnabled(gpu::DISABLE_PROGRAM_DISK_CACHE);
    program_cache_.reset(new gles2::MemoryProgramCache(
        gpu_preferences_.gpu_program_cache_size, disable_disk_cache,
        gpu_feature_info_.IsWorkaroundEnabled(
            gpu::DISABLE_PROGRAM_CACHING_FOR_TRANSFORM_FEEDBACK),
        &activity_flags_));
  }
  return program_cache_.get();
}

InProcessCommandBuffer::InProcessCommandBuffer(
    const scoped_refptr<Service>& service)
    : command_buffer_id_(CommandBufferId::FromUnsafeValue(
          g_next_command_buffer_id.GetNext() + 1)),
      delayed_work_pending_(false),
      image_factory_(nullptr),
      snapshot_requested_(false),
      gpu_control_client_(nullptr),
#if DCHECK_IS_ON()
      context_lost_(false),
#endif
      last_put_offset_(-1),
      gpu_memory_buffer_manager_(nullptr),
      next_fence_sync_release_(1),
      flushed_fence_sync_release_(0),
      flush_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      service_(GetInitialService(service)),
      fence_sync_wait_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED),
      client_thread_weak_ptr_factory_(this),
      gpu_thread_weak_ptr_factory_(this) {
  DCHECK(service_.get());
}

InProcessCommandBuffer::~InProcessCommandBuffer() {
  Destroy();
}

// static
void InProcessCommandBuffer::InitializeDefaultServiceForTesting(
    const GpuFeatureInfo& gpu_feature_info) {
  // Call base::ThreadTaskRunnerHandle::IsSet() to ensure that it is
  // instantiated before we create the GPU thread, otherwise shutdown order will
  // delete the ThreadTaskRunnerHandle before the GPU thread's message loop,
  // and when the message loop is shutdown, it will recreate
  // ThreadTaskRunnerHandle, which will re-add a new task to the, AtExitManager,
  // which causes a deadlock because it's already locked.
  base::ThreadTaskRunnerHandle::IsSet();
  g_default_service.Get().SetGpuFeatureInfo(gpu_feature_info);
}

bool InProcessCommandBuffer::MakeCurrent() {
  CheckSequencedThread();
  command_buffer_lock_.AssertAcquired();

  if (error::IsError(command_buffer_->GetState().error)) {
    DLOG(ERROR) << "MakeCurrent failed because context lost.";
    return false;
  }
  if (!decoder_->MakeCurrent()) {
    DLOG(ERROR) << "Context lost because MakeCurrent failed.";
    command_buffer_->SetParseError(error::kLostContext);
    return false;
  }
  return true;
}

gpu::ContextResult InProcessCommandBuffer::Initialize(
    scoped_refptr<gl::GLSurface> surface,
    bool is_offscreen,
    SurfaceHandle window,
    const gles2::ContextCreationAttribHelper& attribs,
    InProcessCommandBuffer* share_group,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    ImageFactory* image_factory,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!share_group || service_.get() == share_group->service_.get());

  if (surface) {
    // If a surface is provided, we are running in a webview and should not have
    // a task runner.
    DCHECK(!task_runner);

    // GPU thread must be the same as client thread due to GLSurface not being
    // thread safe.
    sequence_checker_.reset(new base::SequenceChecker);
    surface_ = surface;
  } else {
    DCHECK(task_runner);
    origin_task_runner_ = std::move(task_runner);
    client_thread_weak_ptr_ = client_thread_weak_ptr_factory_.GetWeakPtr();
  }

  Capabilities capabilities;
  InitializeOnGpuThreadParams params(is_offscreen, window, attribs,
                                     &capabilities, share_group, image_factory);

  base::Callback<gpu::ContextResult(void)> init_task =
      base::Bind(&InProcessCommandBuffer::InitializeOnGpuThread,
                 base::Unretained(this), params);

  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  QueueTask(true, base::Bind(&RunTaskWithResult<gpu::ContextResult>, init_task,
                             &result, &completion));
  completion.Wait();

  gpu_memory_buffer_manager_ = gpu_memory_buffer_manager;

  if (result == gpu::ContextResult::kSuccess)
    capabilities_ = capabilities;

  return result;
}

gpu::ContextResult InProcessCommandBuffer::InitializeOnGpuThread(
    const InitializeOnGpuThreadParams& params) {
  CheckSequencedThread();
  gpu_thread_weak_ptr_ = gpu_thread_weak_ptr_factory_.GetWeakPtr();

  transfer_buffer_manager_ = std::make_unique<TransferBufferManager>(nullptr);

  gl_share_group_ = params.context_group ? params.context_group->gl_share_group_
                                         : service_->share_group();

  bool bind_generates_resource = false;
  gpu::GpuDriverBugWorkarounds workarounds(
      service_->gpu_feature_info().enabled_gpu_driver_bug_workarounds);
  auto feature_info = base::MakeRefCounted<gles2::FeatureInfo>(workarounds);

  context_group_ =
      params.context_group
          ? params.context_group->decoder_->GetContextGroup()
          : new gles2::ContextGroup(
                service_->gpu_preferences(),
                gles2::PassthroughCommandDecoderSupported(),
                service_->mailbox_manager(), nullptr /* memory_tracker */,
                service_->shader_translator_cache(),
                service_->framebuffer_completeness_cache(), feature_info,
                bind_generates_resource, service_->image_manager(),
                nullptr /* image_factory */, nullptr /* progress_reporter */,
                GpuFeatureInfo(), service_->discardable_manager());

  command_buffer_ = std::make_unique<CommandBufferService>(
      this, transfer_buffer_manager_.get());
  decoder_.reset(gles2::GLES2Decoder::Create(this, command_buffer_.get(),
                                             service_->outputter(),
                                             context_group_.get()));

  if (!surface_) {
    if (params.is_offscreen) {
      surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    } else {
      surface_ = ImageTransportSurface::CreateNativeSurface(
          gpu_thread_weak_ptr_factory_.GetWeakPtr(), params.window,
          gl::GLSurfaceFormat());
      if (!surface_ || !surface_->Initialize(gl::GLSurfaceFormat())) {
        surface_ = nullptr;
        LOG(ERROR) << "ContextResult::kFatalFailure: "
                      "Failed to create surface.";
        return gpu::ContextResult::kFatalFailure;
      }
    }
  }

  if (!surface_.get()) {
    DestroyOnGpuThread();
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "Could not create GLSurface.";
    return gpu::ContextResult::kFatalFailure;
  }

  sync_point_order_data_ =
      service_->sync_point_manager()->CreateSyncPointOrderData();
  sync_point_client_state_ =
      service_->sync_point_manager()->CreateSyncPointClientState(
          GetNamespaceID(), GetCommandBufferID(),
          sync_point_order_data_->sequence_id());

  use_virtualized_gl_context_ =
      service_->UseVirtualizedGLContexts() || decoder_->GetContextGroup()
                                                  ->feature_info()
                                                  ->workarounds()
                                                  .use_virtualized_gl_contexts;

  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLContextIsVirtual,
                                use_virtualized_gl_context_ ? "1" : "0");

  if (use_virtualized_gl_context_) {
    context_ = gl_share_group_->GetSharedContext(surface_.get());
    if (!context_.get()) {
      context_ = gl::init::CreateGLContext(
          gl_share_group_.get(), surface_.get(),
          GenerateGLContextAttribs(params.attribs,
                                   decoder_->GetContextGroup()));
      if (context_.get()) {
        service_->gpu_feature_info().ApplyToGLContext(context_.get());
      }
      gl_share_group_->SetSharedContext(surface_.get(), context_.get());
    }

    context_ = new GLContextVirtual(gl_share_group_.get(), context_.get(),
                                    decoder_->AsWeakPtr());
    if (context_->Initialize(
            surface_.get(), GenerateGLContextAttribs(
                                params.attribs, decoder_->GetContextGroup()))) {
      VLOG(1) << "Created virtual GL context.";
    } else {
      context_ = NULL;
    }
  } else {
    context_ = gl::init::CreateGLContext(
        gl_share_group_.get(), surface_.get(),
        GenerateGLContextAttribs(params.attribs, decoder_->GetContextGroup()));
    if (context_.get()) {
      service_->gpu_feature_info().ApplyToGLContext(context_.get());
    }
  }

  if (!context_.get()) {
    DestroyOnGpuThread();
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "Could not create GLContext.";
    return gpu::ContextResult::kFatalFailure;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    DestroyOnGpuThread();
    // The caller should retry making a context, but this one won't work.
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "Could not make context current.";
    return gpu::ContextResult::kTransientFailure;
  }

  if (!decoder_->GetContextGroup()->has_program_cache() &&
      !decoder_->GetContextGroup()
           ->feature_info()
           ->workarounds()
           .disable_program_cache) {
    decoder_->GetContextGroup()->set_program_cache(service_->program_cache());
  }

  gles2::DisallowedFeatures disallowed_features;
  disallowed_features.gpu_memory_manager = true;
  auto result = decoder_->Initialize(surface_, context_, params.is_offscreen,
                                     disallowed_features, params.attribs);
  if (result != gpu::ContextResult::kSuccess) {
    LOG(ERROR) << "Could not initialize decoder.";
    DestroyOnGpuThread();
    return result;
  }
  *params.capabilities = decoder_->GetCapabilities();

  image_factory_ = params.image_factory;

  return gpu::ContextResult::kSuccess;
}

void InProcessCommandBuffer::Destroy() {
  CheckSequencedThread();
  client_thread_weak_ptr_factory_.InvalidateWeakPtrs();
  gpu_control_client_ = nullptr;
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool result = false;
  base::Callback<bool(void)> destroy_task = base::Bind(
      &InProcessCommandBuffer::DestroyOnGpuThread, base::Unretained(this));
  QueueTask(true, base::Bind(&RunTaskWithResult<bool>, destroy_task, &result,
                             &completion));
  completion.Wait();
}

bool InProcessCommandBuffer::DestroyOnGpuThread() {
  CheckSequencedThread();
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLContextIsVirtual,
                                use_virtualized_gl_context_ ? "1" : "0");
  gpu_thread_weak_ptr_factory_.InvalidateWeakPtrs();
  // Clean up GL resources if possible.
  bool have_context = context_.get() && context_->MakeCurrent(surface_.get());
  if (decoder_) {
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
  command_buffer_.reset();
  context_ = nullptr;
  surface_ = nullptr;
  if (sync_point_order_data_) {
    sync_point_order_data_->Destroy();
    sync_point_order_data_ = nullptr;
  }
  if (sync_point_client_state_) {
    sync_point_client_state_->Destroy();
    sync_point_client_state_ = nullptr;
  }
  gl_share_group_ = nullptr;
  context_group_ = nullptr;

  base::AutoLock lock(task_queue_lock_);
  base::queue<std::unique_ptr<GpuTask>> empty;
  task_queue_.swap(empty);

  return true;
}

void InProcessCommandBuffer::CheckSequencedThread() {
  DCHECK(!sequence_checker_ || sequence_checker_->CalledOnValidSequence());
}

CommandBufferServiceClient::CommandBatchProcessedResult
InProcessCommandBuffer::OnCommandBatchProcessed() {
  return kContinueExecution;
}

void InProcessCommandBuffer::OnParseError() {
  if (!origin_task_runner_)
    return OnContextLost();  // Just kidding, we're on the client thread.
  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&InProcessCommandBuffer::OnContextLost,
                            client_thread_weak_ptr_));
}

void InProcessCommandBuffer::OnContextLost() {
  CheckSequencedThread();

#if DCHECK_IS_ON()
  // This method shouldn't be called more than once.
  DCHECK(!context_lost_);
  context_lost_ = true;
#endif

  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContext();
}

void InProcessCommandBuffer::QueueTask(bool out_of_order,
                                       const base::Closure& task) {
  if (out_of_order) {
    service_->ScheduleTask(task);
    return;
  }
  // Release the |task_queue_lock_| before calling ScheduleTask because
  // the callback may get called immediately and attempt to acquire the lock.
  uint32_t order_num = sync_point_order_data_->GenerateUnprocessedOrderNumber();
  {
    base::AutoLock lock(task_queue_lock_);
    task_queue_.push(std::make_unique<GpuTask>(task, order_num));
  }
  service_->ScheduleTask(base::Bind(
      &InProcessCommandBuffer::ProcessTasksOnGpuThread, gpu_thread_weak_ptr_));
}

void InProcessCommandBuffer::ProcessTasksOnGpuThread() {
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLContextIsVirtual,
                                use_virtualized_gl_context_ ? "1" : "0");
  while (command_buffer_->scheduled()) {
    base::AutoLock lock(task_queue_lock_);
    if (task_queue_.empty())
      break;
    GpuTask* task = task_queue_.front().get();
    sync_point_order_data_->BeginProcessingOrderNumber(task->order_number);
    task->callback.Run();
    if (!command_buffer_->scheduled() &&
        !service_->BlockThreadOnWaitSyncToken()) {
      sync_point_order_data_->PauseProcessingOrderNumber(task->order_number);
      // Don't pop the task if it was preempted - it may have been preempted, so
      // we need to execute it again later.
      return;
    }
    sync_point_order_data_->FinishProcessingOrderNumber(task->order_number);
    task_queue_.pop();
  }
}

CommandBuffer::State InProcessCommandBuffer::GetLastState() {
  CheckSequencedThread();
  base::AutoLock lock(last_state_lock_);
  return last_state_;
}

void InProcessCommandBuffer::UpdateLastStateOnGpuThread() {
  CheckSequencedThread();
  command_buffer_lock_.AssertAcquired();
  base::AutoLock lock(last_state_lock_);
  command_buffer_->UpdateState();
  State state = command_buffer_->GetState();
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

void InProcessCommandBuffer::FlushOnGpuThread(int32_t put_offset,
                                              bool snapshot_requested) {
  CheckSequencedThread();
  ScopedEvent handle_flush(&flush_event_);
  base::AutoLock lock(command_buffer_lock_);

  if (snapshot_requested && snapshot_requested_callback_)
    snapshot_requested_callback_.Run();

  if (!MakeCurrent())
    return;

  command_buffer_->Flush(put_offset, decoder_.get());
  // Update state before signaling the flush event.
  UpdateLastStateOnGpuThread();

  // If we've processed all pending commands but still have pending queries,
  // pump idle work until the query is passed.
  if (put_offset == command_buffer_->GetState().get_offset &&
      (decoder_->HasMoreIdleWork() || decoder_->HasPendingQueries())) {
    ScheduleDelayedWorkOnGpuThread();
  }
}

void InProcessCommandBuffer::PerformDelayedWorkOnGpuThread() {
  CheckSequencedThread();
  delayed_work_pending_ = false;
  base::AutoLock lock(command_buffer_lock_);
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLContextIsVirtual,
                                use_virtualized_gl_context_ ? "1" : "0");
  if (MakeCurrent()) {
    decoder_->PerformIdleWork();
    decoder_->ProcessPendingQueries(false);
    if (decoder_->HasMoreIdleWork() || decoder_->HasPendingQueries()) {
      ScheduleDelayedWorkOnGpuThread();
    }
  }
}

void InProcessCommandBuffer::ScheduleDelayedWorkOnGpuThread() {
  CheckSequencedThread();
  if (delayed_work_pending_)
    return;
  delayed_work_pending_ = true;
  service_->ScheduleDelayedWork(
      base::Bind(&InProcessCommandBuffer::PerformDelayedWorkOnGpuThread,
                 gpu_thread_weak_ptr_));
}

void InProcessCommandBuffer::Flush(int32_t put_offset) {
  CheckSequencedThread();
  if (GetLastState().error != error::kNoError)
    return;

  if (last_put_offset_ == put_offset)
    return;

  last_put_offset_ = put_offset;
  base::Closure task =
      base::Bind(&InProcessCommandBuffer::FlushOnGpuThread,
                 gpu_thread_weak_ptr_, put_offset, snapshot_requested_);
  snapshot_requested_ = false;
  QueueTask(false, task);

  flushed_fence_sync_release_ = next_fence_sync_release_ - 1;
}

void InProcessCommandBuffer::OrderingBarrier(int32_t put_offset) {
  Flush(put_offset);
}

CommandBuffer::State InProcessCommandBuffer::WaitForTokenInRange(int32_t start,
                                                                 int32_t end) {
  CheckSequencedThread();
  State last_state = GetLastState();
  while (!InRange(start, end, last_state.token) &&
         last_state.error == error::kNoError) {
    flush_event_.Wait();
    last_state = GetLastState();
  }
  return last_state;
}

CommandBuffer::State InProcessCommandBuffer::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  CheckSequencedThread();
  State last_state = GetLastState();
  while (((set_get_buffer_count != last_state.set_get_buffer_count) ||
          !InRange(start, end, last_state.get_offset)) &&
         last_state.error == error::kNoError) {
    flush_event_.Wait();
    last_state = GetLastState();
  }
  return last_state;
}

void InProcessCommandBuffer::SetGetBuffer(int32_t shm_id) {
  CheckSequencedThread();
  if (GetLastState().error != error::kNoError)
    return;

  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::Closure task =
      base::Bind(&InProcessCommandBuffer::SetGetBufferOnGpuThread,
                 base::Unretained(this), shm_id, &completion);
  QueueTask(false, task);
  completion.Wait();

  last_put_offset_ = 0;
}

void InProcessCommandBuffer::SetGetBufferOnGpuThread(
    int32_t shm_id,
    base::WaitableEvent* completion) {
  base::AutoLock lock(command_buffer_lock_);
  command_buffer_->SetGetBuffer(shm_id);
  UpdateLastStateOnGpuThread();
  completion->Signal();
}

scoped_refptr<Buffer> InProcessCommandBuffer::CreateTransferBuffer(
    size_t size,
    int32_t* id) {
  CheckSequencedThread();
  base::AutoLock lock(command_buffer_lock_);
  return command_buffer_->CreateTransferBuffer(size, id);
}

void InProcessCommandBuffer::DestroyTransferBuffer(int32_t id) {
  CheckSequencedThread();
  base::Closure task =
      base::Bind(&InProcessCommandBuffer::DestroyTransferBufferOnGpuThread,
                 base::Unretained(this), id);

  QueueTask(false, task);
}

void InProcessCommandBuffer::DestroyTransferBufferOnGpuThread(int32_t id) {
  base::AutoLock lock(command_buffer_lock_);
  command_buffer_->DestroyTransferBuffer(id);
}

void InProcessCommandBuffer::SetGpuControlClient(GpuControlClient* client) {
  gpu_control_client_ = client;
}

const Capabilities& InProcessCommandBuffer::GetCapabilities() const {
  return capabilities_;
}

const GpuFeatureInfo& InProcessCommandBuffer::GetGpuFeatureInfo() const {
  return service_->gpu_feature_info();
}

int32_t InProcessCommandBuffer::CreateImage(ClientBuffer buffer,
                                            size_t width,
                                            size_t height,
                                            unsigned internalformat) {
  CheckSequencedThread();

  DCHECK(gpu_memory_buffer_manager_);
  gfx::GpuMemoryBuffer* gpu_memory_buffer =
      reinterpret_cast<gfx::GpuMemoryBuffer*>(buffer);
  DCHECK(gpu_memory_buffer);

  int32_t new_id = g_next_image_id.GetNext() + 1;

  DCHECK(IsImageFromGpuMemoryBufferFormatSupported(
      gpu_memory_buffer->GetFormat(), capabilities_));
  DCHECK(IsImageFormatCompatibleWithGpuMemoryBufferFormat(
      internalformat, gpu_memory_buffer->GetFormat()));

  // This handle is owned by the GPU thread and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // queuing of the CreateImage task below.
  gfx::GpuMemoryBufferHandle handle =
      gfx::CloneHandleForIPC(gpu_memory_buffer->GetHandle());
  bool requires_sync_point = handle.type == gfx::IO_SURFACE_BUFFER;

  uint64_t fence_sync = 0;
  if (requires_sync_point) {
    fence_sync = GenerateFenceSyncRelease();

    // Previous fence syncs should be flushed already.
    DCHECK_EQ(fence_sync - 1, flushed_fence_sync_release_);
  }

  QueueTask(false, base::Bind(&InProcessCommandBuffer::CreateImageOnGpuThread,
                              base::Unretained(this), new_id, handle,
                              gfx::Size(base::checked_cast<int>(width),
                                        base::checked_cast<int>(height)),
                              gpu_memory_buffer->GetFormat(),
                              base::checked_cast<uint32_t>(internalformat),
                              fence_sync));

  if (fence_sync) {
    flushed_fence_sync_release_ = fence_sync;
    SyncToken sync_token(GetNamespaceID(), 0, GetCommandBufferID(), fence_sync);
    sync_token.SetVerifyFlush();
    gpu_memory_buffer_manager_->SetDestructionSyncToken(gpu_memory_buffer,
                                                        sync_token);
  }

  return new_id;
}

void InProcessCommandBuffer::CreateImageOnGpuThread(
    int32_t id,
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint32_t internalformat,
    uint64_t fence_sync) {
  gles2::ImageManager* image_manager = service_->image_manager();
  DCHECK(image_manager);
  if (image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image already exists with same ID.";
    return;
  }

  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      if (!base::IsValueInRangeForNumericType<size_t>(handle.stride)) {
        LOG(ERROR) << "Invalid stride for image.";
        return;
      }
      scoped_refptr<gl::GLImageSharedMemory> image(
          new gl::GLImageSharedMemory(size, internalformat));
      if (!image->Initialize(handle.handle, handle.id, format, handle.offset,
                             handle.stride)) {
        LOG(ERROR) << "Failed to initialize image.";
        return;
      }

      image_manager->AddImage(image.get(), id);
      break;
    }
    default: {
      if (!image_factory_) {
        LOG(ERROR) << "Image factory missing but required by buffer type.";
        return;
      }

      // Note: this assumes that client ID is always 0.
      const int kClientId = 0;

      scoped_refptr<gl::GLImage> image =
          image_factory_->CreateImageForGpuMemoryBuffer(
              handle, size, format, internalformat, kClientId,
              kNullSurfaceHandle);
      if (!image.get()) {
        LOG(ERROR) << "Failed to create image for buffer.";
        return;
      }

      image_manager->AddImage(image.get(), id);
      break;
    }
  }

  if (fence_sync)
    sync_point_client_state_->ReleaseFenceSync(fence_sync);
}

void InProcessCommandBuffer::DestroyImage(int32_t id) {
  CheckSequencedThread();

  QueueTask(false, base::Bind(&InProcessCommandBuffer::DestroyImageOnGpuThread,
                              base::Unretained(this), id));
}

void InProcessCommandBuffer::DestroyImageOnGpuThread(int32_t id) {
  gles2::ImageManager* image_manager = service_->image_manager();
  DCHECK(image_manager);
  if (!image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image with ID doesn't exist.";
    return;
  }

  image_manager->RemoveImage(id);
}

void InProcessCommandBuffer::OnConsoleMessage(int32_t id,
                                              const std::string& message) {
  // TODO(piman): implement this.
}

void InProcessCommandBuffer::CacheShader(const std::string& key,
                                         const std::string& shader) {
  // TODO(piman): implement this.
}

void InProcessCommandBuffer::OnFenceSyncRelease(uint64_t release) {
  SyncToken sync_token(GetNamespaceID(), 0, GetCommandBufferID(), release);

  gles2::MailboxManager* mailbox_manager =
      decoder_->GetContextGroup()->mailbox_manager();
  mailbox_manager->PushTextureUpdates(sync_token);

  sync_point_client_state_->ReleaseFenceSync(release);
}

bool InProcessCommandBuffer::OnWaitSyncToken(const SyncToken& sync_token) {
  DCHECK(!waiting_for_sync_point_);
  SyncPointManager* sync_point_manager = service_->sync_point_manager();
  DCHECK(sync_point_manager);

  gles2::MailboxManager* mailbox_manager =
      decoder_->GetContextGroup()->mailbox_manager();
  DCHECK(mailbox_manager);

  if (service_->BlockThreadOnWaitSyncToken()) {
    // Wait if sync point wait is valid.
    if (sync_point_client_state_->Wait(
            sync_token,
            base::Bind(&base::WaitableEvent::Signal,
                       base::Unretained(&fence_sync_wait_event_)))) {
      fence_sync_wait_event_.Wait();
    }

    mailbox_manager->PullTextureUpdates(sync_token);
    return false;
  }

  waiting_for_sync_point_ = sync_point_client_state_->Wait(
      sync_token,
      base::Bind(&InProcessCommandBuffer::OnWaitSyncTokenCompleted,
                 gpu_thread_weak_ptr_factory_.GetWeakPtr(), sync_token));
  if (!waiting_for_sync_point_) {
    mailbox_manager->PullTextureUpdates(sync_token);
    return false;
  }

  command_buffer_->SetScheduled(false);
  return true;
}

void InProcessCommandBuffer::OnWaitSyncTokenCompleted(
    const SyncToken& sync_token) {
  DCHECK(waiting_for_sync_point_);
  gles2::MailboxManager* mailbox_manager =
      decoder_->GetContextGroup()->mailbox_manager();
  mailbox_manager->PullTextureUpdates(sync_token);
  waiting_for_sync_point_ = false;
  command_buffer_->SetScheduled(true);
  service_->ScheduleTask(base::Bind(
      &InProcessCommandBuffer::ProcessTasksOnGpuThread, gpu_thread_weak_ptr_));
}

void InProcessCommandBuffer::OnDescheduleUntilFinished() {
  if (!service_->BlockThreadOnWaitSyncToken()) {
    DCHECK(command_buffer_->scheduled());
    DCHECK(decoder_->HasPollingWork());

    command_buffer_->SetScheduled(false);
  }
}

void InProcessCommandBuffer::OnRescheduleAfterFinished() {
  if (!service_->BlockThreadOnWaitSyncToken()) {
    DCHECK(!command_buffer_->scheduled());

    command_buffer_->SetScheduled(true);
    ProcessTasksOnGpuThread();
  }
}

void InProcessCommandBuffer::SignalSyncTokenOnGpuThread(
    const SyncToken& sync_token,
    const base::Closure& callback) {
  if (!sync_point_client_state_->Wait(sync_token, WrapCallback(callback)))
    callback.Run();
}

void InProcessCommandBuffer::SignalQuery(unsigned query_id,
                                         const base::Closure& callback) {
  CheckSequencedThread();
  QueueTask(false, base::Bind(&InProcessCommandBuffer::SignalQueryOnGpuThread,
                              base::Unretained(this), query_id,
                              WrapCallback(callback)));
}

void InProcessCommandBuffer::SignalQueryOnGpuThread(
    unsigned query_id,
    const base::Closure& callback) {
  gles2::QueryManager* query_manager_ = decoder_->GetQueryManager();
  DCHECK(query_manager_);

  gles2::QueryManager::Query* query = query_manager_->GetQuery(query_id);
  if (!query)
    callback.Run();
  else
    query->AddCallback(callback);
}

void InProcessCommandBuffer::SetLock(base::Lock*) {
  // No support for using on multiple threads.
  NOTREACHED();
}

void InProcessCommandBuffer::EnsureWorkVisible() {
  // This is only relevant for out-of-process command buffers.
}

CommandBufferNamespace InProcessCommandBuffer::GetNamespaceID() const {
  return CommandBufferNamespace::IN_PROCESS;
}

CommandBufferId InProcessCommandBuffer::GetCommandBufferID() const {
  return command_buffer_id_;
}

void InProcessCommandBuffer::FlushPendingWork() {
  // This is only relevant for out-of-process command buffers.
}

uint64_t InProcessCommandBuffer::GenerateFenceSyncRelease() {
  return next_fence_sync_release_++;
}

bool InProcessCommandBuffer::IsFenceSyncRelease(uint64_t release) {
  return release != 0 && release < next_fence_sync_release_;
}

bool InProcessCommandBuffer::IsFenceSyncFlushed(uint64_t release) {
  return release <= flushed_fence_sync_release_;
}

bool InProcessCommandBuffer::IsFenceSyncFlushReceived(uint64_t release) {
  return IsFenceSyncFlushed(release);
}

bool InProcessCommandBuffer::IsFenceSyncReleased(uint64_t release) {
  return release <= GetLastState().release_count;
}

void InProcessCommandBuffer::SignalSyncToken(const SyncToken& sync_token,
                                             const base::Closure& callback) {
  CheckSequencedThread();
  QueueTask(
      false,
      base::Bind(&InProcessCommandBuffer::SignalSyncTokenOnGpuThread,
                 base::Unretained(this), sync_token, WrapCallback(callback)));
}

void InProcessCommandBuffer::WaitSyncTokenHint(const SyncToken& sync_token) {}

bool InProcessCommandBuffer::CanWaitUnverifiedSyncToken(
    const SyncToken& sync_token) {
  return sync_token.namespace_id() == GetNamespaceID();
}

void InProcessCommandBuffer::SetSnapshotRequested() {
  snapshot_requested_ = true;
}

#if defined(OS_WIN)
void InProcessCommandBuffer::DidCreateAcceleratedSurfaceChildWindow(
    SurfaceHandle parent_window,
    SurfaceHandle child_window) {
  ::SetParent(child_window, parent_window);
}
#endif

void InProcessCommandBuffer::DidSwapBuffersComplete(
    SwapBuffersCompleteParams params) {
  if (!origin_task_runner_) {
    DidSwapBuffersCompleteOnOriginThread(std::move(params));
    return;
  }
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InProcessCommandBuffer::DidSwapBuffersCompleteOnOriginThread,
                 client_thread_weak_ptr_, base::Passed(&params)));
}

const gles2::FeatureInfo* InProcessCommandBuffer::GetFeatureInfo() const {
  return context_group_->feature_info();
}

const GpuPreferences& InProcessCommandBuffer::GetGpuPreferences() const {
  return context_group_->gpu_preferences();
}

void InProcessCommandBuffer::UpdateVSyncParameters(base::TimeTicks timebase,
                                                   base::TimeDelta interval) {
  if (!origin_task_runner_) {
    UpdateVSyncParametersOnOriginThread(timebase, interval);
    return;
  }
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InProcessCommandBuffer::UpdateVSyncParametersOnOriginThread,
                 client_thread_weak_ptr_, timebase, interval));
}

void InProcessCommandBuffer::SetSnapshotRequestedCallback(
    const base::Closure& callback) {
  snapshot_requested_callback_ = callback;
}

void InProcessCommandBuffer::BufferPresented(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  if (!origin_task_runner_) {
    BufferPresentedOnOriginThread(swap_id, feedback);
    return;
  }
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InProcessCommandBuffer::BufferPresentedOnOriginThread,
                 client_thread_weak_ptr_, swap_id, feedback));
}

void InProcessCommandBuffer::AddFilter(IPC::MessageFilter* message_filter) {
  NOTREACHED();
}

int32_t InProcessCommandBuffer::GetRouteID() const {
  NOTREACHED();
  return 0;
}

void InProcessCommandBuffer::DidSwapBuffersCompleteOnOriginThread(
    SwapBuffersCompleteParams params) {
#if defined(OS_MACOSX)
  GpuProcessHostedCALayerTreeParamsMac params_mac;
  params_mac.ca_context_id = params.ca_context_id;
  params_mac.fullscreen_low_power_ca_context_valid =
      params.fullscreen_low_power_ca_context_valid;
  params_mac.fullscreen_low_power_ca_context_id =
      params.fullscreen_low_power_ca_context_id;
  params_mac.io_surface.reset(IOSurfaceLookupFromMachPort(params.io_surface));
  params_mac.pixel_size = params.pixel_size;
  params_mac.scale_factor = params.scale_factor;
  params_mac.responses = std::move(params.in_use_responses);
  GpuProcessHostedCALayerTreeParamsMac* mac_frame_ptr = &params_mac;
#else
  GpuProcessHostedCALayerTreeParamsMac* mac_frame_ptr = nullptr;
#endif
  if (!swap_buffers_completion_callback_.is_null())
    swap_buffers_completion_callback_.Run(params.response, mac_frame_ptr);
}

void InProcessCommandBuffer::UpdateVSyncParametersOnOriginThread(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePresentationCallback));
  if (!update_vsync_parameters_completion_callback_.is_null())
    update_vsync_parameters_completion_callback_.Run(timebase, interval);
}

void InProcessCommandBuffer::BufferPresentedOnOriginThread(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePresentationCallback));
  if (presentation_callback_)
    presentation_callback_.Run(swap_id, feedback);
  if (update_vsync_parameters_completion_callback_ &&
      feedback.timestamp != base::TimeTicks())
    update_vsync_parameters_completion_callback_.Run(feedback.timestamp,
                                                     feedback.interval);
}

void InProcessCommandBuffer::SetSwapBuffersCompletionCallback(
    const SwapBuffersCompletionCallback& callback) {
  swap_buffers_completion_callback_ = callback;
}

void InProcessCommandBuffer::SetUpdateVSyncParametersCallback(
    const UpdateVSyncParametersCallback& callback) {
  update_vsync_parameters_completion_callback_ = callback;
}

void InProcessCommandBuffer::SetPresentationCallback(
    const PresentationCallback& callback) {
  presentation_callback_ = callback;
}

namespace {

void PostCallback(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::Closure& callback) {
  // The task_runner.get() check is to support using InProcessCommandBuffer on
  // a thread without a message loop.
  if (task_runner.get() && !task_runner->BelongsToCurrentThread()) {
    task_runner->PostTask(FROM_HERE, callback);
  } else {
    callback.Run();
  }
}

void RunOnTargetThread(std::unique_ptr<base::Closure> callback) {
  DCHECK(callback.get());
  callback->Run();
}

}  // anonymous namespace

base::Closure InProcessCommandBuffer::WrapCallback(
    const base::Closure& callback) {
  // Make sure the callback gets deleted on the target thread by passing
  // ownership.
  std::unique_ptr<base::Closure> scoped_callback(new base::Closure(callback));
  base::Closure callback_on_client_thread =
      base::Bind(&RunOnTargetThread, base::Passed(&scoped_callback));
  base::Closure wrapped_callback =
      base::Bind(&PostCallback, base::ThreadTaskRunnerHandle::IsSet()
                                    ? base::ThreadTaskRunnerHandle::Get()
                                    : nullptr,
                 callback_on_client_thread);
  return wrapped_callback;
}

InProcessCommandBuffer::GpuTask::GpuTask(const base::Closure& callback,
                                         uint32_t order_number)
    : callback(callback), order_number(order_number) {}

InProcessCommandBuffer::GpuTask::~GpuTask() {}

}  // namespace gpu
