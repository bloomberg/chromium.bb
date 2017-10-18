// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/gpu_service_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/memory/shared_memory.h"
#include "base/run_loop.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/in_process_context_provider.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/dx_diag_node.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator_factory_provider.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_encode_accelerator.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "media/mojo/services/mojo_video_encode_accelerator_provider.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/throw_uncaught_exception.h"
#include "media/gpu/android/content_video_view_overlay_allocator.h"
#endif

#if defined(OS_WIN)
#include "gpu/ipc/service/direct_composition_surface_win.h"
#endif

namespace viz {

namespace {

static base::LazyInstance<base::Callback<
    void(int severity, size_t message_start, const std::string& message)>>::
    Leaky g_log_callback = LAZY_INSTANCE_INITIALIZER;

bool GpuLogMessageHandler(int severity,
                          const char* file,
                          int line,
                          size_t message_start,
                          const std::string& message) {
  g_log_callback.Get().Run(severity, message_start, message);
  return false;
}

// Returns a callback which does a PostTask to run |callback| on the |runner|
// task runner.
template <typename Param>
base::OnceCallback<void(const Param&)> WrapCallback(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    base::OnceCallback<void(const Param&)> callback) {
  return base::BindOnce(
      [](base::SingleThreadTaskRunner* runner,
         base::OnceCallback<void(const Param&)> callback, const Param& param) {
        runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), param));
      },
      base::RetainedRef(std::move(runner)), std::move(callback));
}

void DestroyBinding(mojo::BindingSet<mojom::GpuService>* binding,
                    base::WaitableEvent* wait) {
  binding->CloseAllBindings();
  wait->Signal();
}

}  // namespace

GpuServiceImpl::GpuServiceImpl(
    const gpu::GPUInfo& gpu_info,
    std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::GpuPreferences& gpu_preferences)
    : main_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_runner_(std::move(io_runner)),
      watchdog_thread_(std::move(watchdog_thread)),
      gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType()),
      gpu_preferences_(gpu_preferences),
      gpu_info_(gpu_info),
      gpu_feature_info_(gpu_feature_info),
      bindings_(base::MakeUnique<mojo::BindingSet<mojom::GpuService>>()),
      weak_ptr_factory_(this) {
  DCHECK(!io_runner_->BelongsToCurrentThread());
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

GpuServiceImpl::~GpuServiceImpl() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  bind_task_tracker_.TryCancelAll();
  logging::SetLogMessageHandler(nullptr);
  g_log_callback.Get() =
      base::Callback<void(int, size_t, const std::string&)>();
  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (io_runner_->PostTask(
          FROM_HERE, base::Bind(&DestroyBinding, bindings_.get(), &wait))) {
    wait.Wait();
  }
  media_gpu_channel_manager_.reset();
  gpu_channel_manager_.reset();
  owned_sync_point_manager_.reset();

  // Signal this event before destroying the child process. That way all
  // background threads can cleanup. For example, in the renderer the
  // RenderThread instances will be able to notice shutdown before the render
  // process begins waiting for them to exit.
  if (owned_shutdown_event_)
    owned_shutdown_event_->Signal();
}

void GpuServiceImpl::UpdateGPUInfo() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  DCHECK(!gpu_host_);
  gpu::GpuDriverBugWorkarounds gpu_workarounds(
      gpu_feature_info_.enabled_gpu_driver_bug_workarounds);
  gpu_info_.video_decode_accelerator_capabilities =
      media::GpuVideoDecodeAccelerator::GetCapabilities(gpu_preferences_,
                                                        gpu_workarounds);
  gpu_info_.video_encode_accelerator_supported_profiles =
      media::GpuVideoEncodeAccelerator::GetSupportedProfiles(gpu_preferences_);
  gpu_info_.jpeg_decode_accelerator_supported =
      media::GpuJpegDecodeAcceleratorFactoryProvider::
          IsAcceleratedJpegDecodeSupported();
  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  gpu_info_.initialization_time = base::Time::Now() - start_time_;
}

void GpuServiceImpl::InitializeWithHost(
    mojom::GpuHostPtr gpu_host,
    gpu::GpuProcessActivityFlags activity_flags,
    gpu::SyncPointManager* sync_point_manager,
    base::WaitableEvent* shutdown_event) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu_host->DidInitialize(gpu_info_, gpu_feature_info_);
  gpu_host_ =
      mojom::ThreadSafeGpuHostPtr::Create(gpu_host.PassInterface(), io_runner_);
  if (!in_host_process()) {
    // The global callback is reset from the dtor. So Unretained() here is safe.
    // Note that the callback can be called from any thread. Consequently, the
    // callback cannot use a WeakPtr.
    g_log_callback.Get() =
        base::Bind(&GpuServiceImpl::RecordLogMessage, base::Unretained(this));
    logging::SetLogMessageHandler(GpuLogMessageHandler);
  }

  sync_point_manager_ = sync_point_manager;
  if (!sync_point_manager_) {
    owned_sync_point_manager_ = base::MakeUnique<gpu::SyncPointManager>();
    sync_point_manager_ = owned_sync_point_manager_.get();
  }

  shutdown_event_ = shutdown_event;
  if (!shutdown_event_) {
    owned_shutdown_event_ = base::MakeUnique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    shutdown_event_ = owned_shutdown_event_.get();
  }

  if (gpu_preferences_.enable_gpu_scheduler) {
    scheduler_ = base::MakeUnique<gpu::Scheduler>(
        base::ThreadTaskRunnerHandle::Get(), sync_point_manager_);
  }

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_.reset(new gpu::GpuChannelManager(
      gpu_preferences_, this, watchdog_thread_.get(), main_runner_, io_runner_,
      scheduler_.get(), sync_point_manager_, gpu_memory_buffer_factory_.get(),
      gpu_feature_info_, std::move(activity_flags)));

  media_gpu_channel_manager_.reset(
      new media::MediaGpuChannelManager(gpu_channel_manager_.get()));
  if (watchdog_thread())
    watchdog_thread()->AddPowerObserver();
}

void GpuServiceImpl::Bind(mojom::GpuServiceRequest request) {
  if (main_runner_->BelongsToCurrentThread()) {
    bind_task_tracker_.PostTask(
        io_runner_.get(), FROM_HERE,
        base::Bind(&GpuServiceImpl::Bind, base::Unretained(this),
                   base::Passed(std::move(request))));
    return;
  }
  bindings_->AddBinding(this, std::move(request));
}

gpu::ImageFactory* GpuServiceImpl::gpu_image_factory() {
  return gpu_memory_buffer_factory_
             ? gpu_memory_buffer_factory_->AsImageFactory()
             : nullptr;
}

void GpuServiceImpl::RecordLogMessage(int severity,
                                      size_t message_start,
                                      const std::string& str) {
  // This can be run from any thread.
  std::string header = str.substr(0, message_start);
  std::string message = str.substr(message_start);
  (*gpu_host_)->RecordLogMessage(severity, header, message);
}

void GpuServiceImpl::CreateJpegDecodeAccelerator(
    media::mojom::GpuJpegDecodeAcceleratorRequest jda_request) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  // TODO(c.padhi): Implement this, see https://crbug.com/699255.
  NOTIMPLEMENTED();
}

void GpuServiceImpl::CreateVideoEncodeAcceleratorProvider(
    media::mojom::VideoEncodeAcceleratorProviderRequest vea_provider_request) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  media::MojoVideoEncodeAcceleratorProvider::Create(
      std::move(vea_provider_request),
      base::Bind(&media::GpuVideoEncodeAcceleratorFactory::CreateVEA),
      gpu_preferences_);
}

void GpuServiceImpl::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gpu::SurfaceHandle surface_handle,
    CreateGpuMemoryBufferCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  // This needs to happen in the IO thread.
  std::move(callback).Run(gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
      id, size, format, usage, client_id, surface_handle));
}

void GpuServiceImpl::DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                            int client_id,
                                            const gpu::SyncToken& sync_token) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(FROM_HERE,
                           base::Bind(&GpuServiceImpl::DestroyGpuMemoryBuffer,
                                      weak_ptr_, id, client_id, sync_token));
    return;
  }
  gpu_channel_manager_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

void GpuServiceImpl::GetVideoMemoryUsageStats(
    GetVideoMemoryUsageStatsCallback callback) {
  if (io_runner_->BelongsToCurrentThread()) {
    auto wrap_callback = WrapCallback(io_runner_, std::move(callback));
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::GetVideoMemoryUsageStats,
                                  weak_ptr_, std::move(wrap_callback)));
    return;
  }
  gpu::VideoMemoryUsageStats video_memory_usage_stats;
  gpu_channel_manager_->gpu_memory_manager()->GetVideoMemoryUsageStats(
      &video_memory_usage_stats);
  std::move(callback).Run(video_memory_usage_stats);
}

void GpuServiceImpl::RequestCompleteGpuInfo(
    RequestCompleteGpuInfoCallback callback) {
  if (io_runner_->BelongsToCurrentThread()) {
    auto wrap_callback = WrapCallback(io_runner_, std::move(callback));
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::RequestCompleteGpuInfo,
                                  weak_ptr_, std::move(wrap_callback)));
    return;
  }
  DCHECK(main_runner_->BelongsToCurrentThread());

  UpdateGpuInfoPlatform(base::BindOnce(
      IgnoreResult(&base::TaskRunner::PostTask), main_runner_, FROM_HERE,
      base::BindOnce(
          [](GpuServiceImpl* gpu_service,
             RequestCompleteGpuInfoCallback callback) {
            std::move(callback).Run(gpu_service->gpu_info_);
#if defined(OS_WIN)
            if (!gpu_service->in_host_process()) {
              // The unsandboxed GPU process fulfilled its duty. Rest
              // in peace.
              base::RunLoop::QuitCurrentWhenIdleDeprecated();
            }
#endif
          },
          this, std::move(callback))));
}

void GpuServiceImpl::RequestHDRStatus(RequestHDRStatusCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuServiceImpl::RequestHDRStatusOnMainThread,
                                weak_ptr_, std::move(callback)));
}

void GpuServiceImpl::RequestHDRStatusOnMainThread(
    RequestHDRStatusCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  bool hdr_enabled = false;
#if defined(OS_WIN)
  hdr_enabled = gpu::DirectCompositionSurfaceWin::IsHDRSupported();
#endif
  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(std::move(callback), hdr_enabled));
}

#if defined(OS_MACOSX)
void GpuServiceImpl::UpdateGpuInfoPlatform(
    base::OnceClosure on_gpu_info_updated) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  // gpu::CollectContextGraphicsInfo() is already called during gpu process
  // initialization (see GpuInit::InitializeAndStartSandbox()) on non-mac
  // platforms, and during in-browser gpu thread initialization on all platforms
  // (See InProcessGpuThread::Init()).
  if (in_host_process())
    return;

  DCHECK_EQ(gpu::kCollectInfoNone, gpu_info_.context_info_state);
  gpu::CollectInfoResult result = gpu::CollectContextGraphicsInfo(&gpu_info_);
  switch (result) {
    case gpu::kCollectInfoFatalFailure:
      LOG(ERROR) << "gpu::CollectGraphicsInfo failed (fatal).";
      // TODO(piman): can we signal overall failure?
      break;
    case gpu::kCollectInfoNonFatalFailure:
      DVLOG(1) << "gpu::CollectGraphicsInfo failed (non-fatal).";
      break;
    case gpu::kCollectInfoNone:
      NOTREACHED();
      break;
    case gpu::kCollectInfoSuccess:
      break;
  }
  gpu::SetKeysForCrashLogging(gpu_info_);
  std::move(on_gpu_info_updated).Run();
}
#elif defined(OS_WIN)
void GpuServiceImpl::UpdateGpuInfoPlatform(
    base::OnceClosure on_gpu_info_updated) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  // GPU full info collection should only happen on un-sandboxed GPU process
  // or single process/in-process gpu mode on Windows.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line->HasSwitch("disable-gpu-sandbox") || in_host_process());

  // We can continue on shutdown here because we're not writing any critical
  // state in this task.
  base::PostTaskAndReplyWithResult(
      base::CreateCOMSTATaskRunnerWithTraits(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
          .get(),
      FROM_HERE, base::BindOnce([]() {
        gpu::DxDiagNode dx_diag_node;
        gpu::GetDxDiagnostics(&dx_diag_node);
        return dx_diag_node;
      }),
      base::BindOnce(
          [](GpuServiceImpl* gpu_service, base::OnceClosure on_gpu_info_updated,
             const gpu::DxDiagNode& dx_diag_node) {
            gpu_service->gpu_info_.dx_diagnostics = dx_diag_node;
            gpu_service->gpu_info_.dx_diagnostics_info_state =
                gpu::kCollectInfoSuccess;
            std::move(on_gpu_info_updated).Run();
          },
          this, std::move(on_gpu_info_updated)));
}
#else
void GpuServiceImpl::UpdateGpuInfoPlatform(
    base::OnceClosure on_gpu_info_updated) {
  std::move(on_gpu_info_updated).Run();
}
#endif

void GpuServiceImpl::DidCreateOffscreenContext(const GURL& active_url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  (*gpu_host_)->DidCreateOffscreenContext(active_url);
}

void GpuServiceImpl::DidDestroyChannel(int client_id) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  media_gpu_channel_manager_->RemoveChannel(client_id);
  (*gpu_host_)->DidDestroyChannel(client_id);
}

void GpuServiceImpl::DidDestroyOffscreenContext(const GURL& active_url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  (*gpu_host_)->DidDestroyOffscreenContext(active_url);
}

void GpuServiceImpl::DidLoseContext(bool offscreen,
                                    gpu::error::ContextLostReason reason,
                                    const GURL& active_url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  (*gpu_host_)->DidLoseContext(offscreen, reason, active_url);
}

void GpuServiceImpl::StoreShaderToDisk(int client_id,
                                       const std::string& key,
                                       const std::string& shader) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  (*gpu_host_)->StoreShaderToDisk(client_id, key, shader);
}

#if defined(OS_WIN)
void GpuServiceImpl::SendAcceleratedSurfaceCreatedChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  (*gpu_host_)->SetChildSurface(parent_window, child_window);
}
#endif

void GpuServiceImpl::SetActiveURL(const GURL& url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  constexpr char kActiveURL[] = "url-chunk";
  base::debug::SetCrashKeyValue(kActiveURL, url.possibly_invalid_spec());
}

void GpuServiceImpl::EstablishGpuChannel(int32_t client_id,
                                         uint64_t client_tracing_id,
                                         bool is_gpu_host,
                                         EstablishGpuChannelCallback callback) {
  if (io_runner_->BelongsToCurrentThread()) {
    EstablishGpuChannelCallback wrap_callback = base::BindOnce(
        [](scoped_refptr<base::SingleThreadTaskRunner> runner,
           EstablishGpuChannelCallback cb,
           mojo::ScopedMessagePipeHandle handle) {
          runner->PostTask(FROM_HERE,
                           base::BindOnce(std::move(cb), std::move(handle)));
        },
        io_runner_, std::move(callback));
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::EstablishGpuChannel,
                                  weak_ptr_, client_id, client_tracing_id,
                                  is_gpu_host, std::move(wrap_callback)));
    return;
  }

  gpu::GpuChannel* gpu_channel = gpu_channel_manager_->EstablishChannel(
      client_id, client_tracing_id, is_gpu_host);

  mojo::MessagePipe pipe;
  gpu_channel->Init(base::MakeUnique<gpu::SyncChannelFilteredSender>(
      pipe.handle0.release(), gpu_channel, io_runner_, shutdown_event_));

  media_gpu_channel_manager_->AddChannel(client_id);

  std::move(callback).Run(std::move(pipe.handle1));
}

void GpuServiceImpl::CloseChannel(int32_t client_id) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(FROM_HERE, base::Bind(&GpuServiceImpl::CloseChannel,
                                                 weak_ptr_, client_id));
    return;
  }
  gpu_channel_manager_->RemoveChannel(client_id);
}

void GpuServiceImpl::LoadedShader(const std::string& key,
                                  const std::string& data) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(FROM_HERE, base::Bind(&GpuServiceImpl::LoadedShader,
                                                 weak_ptr_, key, data));
    return;
  }
  gpu_channel_manager_->PopulateShaderCache(key, data);
}

void GpuServiceImpl::DestroyingVideoSurface(
    int32_t surface_id,
    DestroyingVideoSurfaceCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
#if defined(OS_ANDROID)
  main_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](int32_t surface_id) {
            media::ContentVideoViewOverlayAllocator::GetInstance()
                ->OnSurfaceDestroyed(surface_id);
          },
          surface_id),
      std::move(callback));
#else
  NOTREACHED() << "DestroyingVideoSurface() not supported on this platform.";
#endif
}

void GpuServiceImpl::WakeUpGpu() {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(FROM_HERE,
                           base::Bind(&GpuServiceImpl::WakeUpGpu, weak_ptr_));
    return;
  }
#if defined(OS_ANDROID)
  gpu_channel_manager_->WakeUpGpu();
#else
  NOTREACHED() << "WakeUpGpu() not supported on this platform.";
#endif
}

void GpuServiceImpl::GpuSwitched() {
  DVLOG(1) << "GPU: GPU has switched";
  if (!in_host_process())
    ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched();
}

void GpuServiceImpl::DestroyAllChannels() {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::Bind(&GpuServiceImpl::DestroyAllChannels, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: Removing all contexts";
  gpu_channel_manager_->DestroyAllChannels();
}

void GpuServiceImpl::Crash() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  DVLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuServiceImpl::Hang() {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(FROM_HERE, base::Bind([] {
                           DVLOG(1) << "GPU: Simulating GPU hang";
                           for (;;) {
                             // Do not sleep here. The GPU watchdog timer tracks
                             // the amount of user time this thread is using and
                             // it doesn't use much while calling Sleep.
                           }
                         }));
}

void GpuServiceImpl::ThrowJavaException() {
  DCHECK(io_runner_->BelongsToCurrentThread());
#if defined(OS_ANDROID)
  main_runner_->PostTask(
      FROM_HERE, base::Bind([] { base::android::ThrowUncaughtException(); }));
#else
  NOTREACHED() << "Java exception not supported on this platform.";
#endif
}

void GpuServiceImpl::Stop(StopCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce([] { base::RunLoop::QuitCurrentWhenIdleDeprecated(); }),
      std::move(callback));
}

}  // namespace viz
