// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/main/viz_main_impl.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/service/display_embedder/gpu_display_provider.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/gpu_preferences_util.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

std::unique_ptr<base::Thread> CreateAndStartCompositorThread() {
  auto thread = std::make_unique<base::Thread>("CompositorThread");
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_DEFAULT;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  CHECK(thread->StartWithOptions(thread_options));
  return thread;
}

std::unique_ptr<base::Thread> CreateAndStartIOThread() {
  // TODO(sad): We do not need the IO thread once gpu has a separate process.
  // It should be possible to use |main_task_runner_| for doing IO tasks.
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // TODO(reveman): Remove this in favor of setting it explicitly for each
  // type of process.
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  auto io_thread = std::make_unique<base::Thread>("GpuIOThread");
  CHECK(io_thread->StartWithOptions(thread_options));
  return io_thread;
}

}  // namespace

namespace viz {

VizMainImpl::ExternalDependencies::ExternalDependencies() = default;

VizMainImpl::ExternalDependencies::~ExternalDependencies() = default;

VizMainImpl::ExternalDependencies::ExternalDependencies(
    ExternalDependencies&& other) = default;

VizMainImpl::ExternalDependencies& VizMainImpl::ExternalDependencies::operator=(
    ExternalDependencies&& other) = default;

VizMainImpl::VizMainImpl(Delegate* delegate,
                         ExternalDependencies dependencies,
                         std::unique_ptr<gpu::GpuInit> gpu_init)
    : delegate_(delegate),
      dependencies_(std::move(dependencies)),
      gpu_init_(std::move(gpu_init)),
      gpu_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this),
      associated_binding_(this) {
  // TODO(crbug.com/609317): Remove this when Mus Window Server and GPU are
  // split into separate processes. Until then this is necessary to be able to
  // run Mushrome (chrome --mus) with Mus running in the browser process.
  if (!base::PowerMonitor::Get()) {
    power_monitor_ = base::MakeUnique<base::PowerMonitor>(
        base::MakeUnique<base::PowerMonitorDeviceSource>());
  }

  if (!gpu_init_) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    gpu::GpuPreferences gpu_preferences;
    if (command_line->HasSwitch(switches::kGpuPreferences)) {
      std::string value =
          command_line->GetSwitchValueASCII(switches::kGpuPreferences);
      bool success = gpu::SwitchValueToGpuPreferences(value, &gpu_preferences);
      CHECK(success);
    }
    // Initialize GpuInit before starting the IO or compositor threads.
    gpu_init_ = std::make_unique<gpu::GpuInit>();
    gpu_init_->set_sandbox_helper(this);
    // TODO(crbug.com/609317): Use InitializeAndStartSandbox() when gpu-mus is
    // split into a separate process.
    gpu_init_->InitializeInProcess(command_line, gpu_preferences);
  }

  if (!dependencies_.io_thread_task_runner)
    io_thread_ = CreateAndStartIOThread();
  if (dependencies_.create_display_compositor) {
    compositor_thread_ = CreateAndStartCompositorThread();
    compositor_thread_task_runner_ = compositor_thread_->task_runner();
  }

  CreateUkmRecorderIfNeeded(dependencies.connector);

  gpu_service_ = base::MakeUnique<GpuServiceImpl>(
      gpu_init_->gpu_info(), gpu_init_->TakeWatchdogThread(),
      io_thread_ ? io_thread_->task_runner()
                 : dependencies_.io_thread_task_runner,
      gpu_init_->gpu_feature_info(), gpu_init_->gpu_preferences());
}

VizMainImpl::~VizMainImpl() {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  if (ukm_recorder_)
    ukm::DelegatingUkmRecorder::Get()->RemoveDelegate(ukm_recorder_.get());
  if (io_thread_)
    io_thread_->Stop();
}

void VizMainImpl::SetLogMessagesForHost(LogMessages log_messages) {
  log_messages_ = std::move(log_messages);
}

void VizMainImpl::Bind(mojom::VizMainRequest request) {
  binding_.Bind(std::move(request));
}

void VizMainImpl::BindAssociated(mojom::VizMainAssociatedRequest request) {
  associated_binding_.Bind(std::move(request));
}

void VizMainImpl::TearDown() {
  DCHECK(!gpu_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!compositor_thread_task_runner_->BelongsToCurrentThread());
  // The compositor holds on to some resources from gpu service. So destroy the
  // compositor first, before destroying the gpu service. However, before the
  // compositor is destroyed, close the binding, so that the gpu service doesn't
  // need to process commands from the compositor as it is shutting down.
  base::WaitableEvent binding_wait(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VizMainImpl::CloseVizMainBindingOnGpuThread,
                            base::Unretained(this), &binding_wait));
  binding_wait.Wait();

  base::WaitableEvent compositor_wait(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  compositor_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VizMainImpl::TearDownOnCompositorThread,
                            base::Unretained(this), &compositor_wait));
  compositor_wait.Wait();

  base::WaitableEvent gpu_wait(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VizMainImpl::TearDownOnGpuThread,
                            base::Unretained(this), &gpu_wait));
  gpu_wait.Wait();
}

void VizMainImpl::CreateGpuService(
    mojom::GpuServiceRequest request,
    mojom::GpuHostPtr gpu_host,
    mojo::ScopedSharedBufferHandle activity_flags) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  gpu_service_->UpdateGPUInfo();
  for (const LogMessage& log : log_messages_)
    gpu_host->RecordLogMessage(log.severity, log.header, log.message);
  log_messages_.clear();
  if (!gpu_init_->init_successful()) {
    LOG(ERROR) << "Exiting GPU process due to errors during initialization";
    gpu_service_.reset();
    gpu_host->DidFailInitialize();
    if (delegate_)
      delegate_->OnInitializationFailed();
    return;
  }

  gpu_service_->Bind(std::move(request));
  gpu_service_->InitializeWithHost(
      std::move(gpu_host),
      gpu::GpuProcessActivityFlags(std::move(activity_flags)),
      dependencies_.sync_point_manager, dependencies_.shutdown_event);

  if (!pending_frame_sink_manager_params_.is_null()) {
    CreateFrameSinkManagerInternal(
        std::move(pending_frame_sink_manager_params_));
    pending_frame_sink_manager_params_.reset();
  }
  if (delegate_)
    delegate_->OnGpuServiceConnection(gpu_service_.get());
}

void VizMainImpl::CreateUkmRecorderIfNeeded(
    service_manager::Connector* connector) {
  // If GPU is running in the browser process, we can use browser's UKMRecorder.
  if (gpu_init_->gpu_info().in_process_gpu)
    return;

  DCHECK(connector) << "Unable to initialize UKMRecorder in the GPU process - "
                    << "no valid connector.";
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(connector);
  ukm::DelegatingUkmRecorder::Get()->AddDelegate(ukm_recorder_->GetWeakPtr());
}

void VizMainImpl::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params) {
  DCHECK(compositor_thread_task_runner_);
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  if (!gpu_service_ || !gpu_service_->is_initialized()) {
    DCHECK(pending_frame_sink_manager_params_.is_null());
    pending_frame_sink_manager_params_ = std::move(params);
    return;
  }
  CreateFrameSinkManagerInternal(std::move(params));
}

void VizMainImpl::CreateFrameSinkManagerInternal(
    mojom::FrameSinkManagerParamsPtr params) {
  DCHECK(!gpu_command_service_);
  DCHECK(gpu_service_);
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  gpu_command_service_ = base::MakeRefCounted<gpu::GpuInProcessThreadService>(
      gpu_thread_task_runner_, gpu_service_->sync_point_manager(),
      gpu_service_->mailbox_manager(), gpu_service_->share_group(),
      gpu_service_->gpu_feature_info());

  compositor_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VizMainImpl::CreateFrameSinkManagerOnCompositorThread,
                 base::Unretained(this), base::Passed(&params)));
}

void VizMainImpl::CreateFrameSinkManagerOnCompositorThread(
    mojom::FrameSinkManagerParamsPtr params) {
  DCHECK(!frame_sink_manager_);
  mojom::FrameSinkManagerClientPtr client;
  client.Bind(std::move(params->frame_sink_manager_client));

  display_provider_ = base::MakeUnique<GpuDisplayProvider>(
      params->restart_id, gpu_command_service_,
      gpu_service_->gpu_channel_manager());

  frame_sink_manager_ = base::MakeUnique<FrameSinkManagerImpl>(
      SurfaceManager::LifetimeType::REFERENCES, display_provider_.get());
  frame_sink_manager_->BindAndSetClient(std::move(params->frame_sink_manager),
                                        nullptr, std::move(client));
}

void VizMainImpl::CloseVizMainBindingOnGpuThread(base::WaitableEvent* wait) {
  binding_.Close();
  wait->Signal();
}

void VizMainImpl::TearDownOnCompositorThread(base::WaitableEvent* wait) {
  frame_sink_manager_.reset();
  display_provider_.reset();
  wait->Signal();
}

void VizMainImpl::TearDownOnGpuThread(base::WaitableEvent* wait) {
  gpu_command_service_ = nullptr;
  gpu_service_.reset();
  gpu_memory_buffer_factory_.reset();
  gpu_init_.reset();
  wait->Signal();
}

void VizMainImpl::PreSandboxStartup() {
  // TODO(sad): https://crbug.com/645602
}

bool VizMainImpl::EnsureSandboxInitialized(
    gpu::GpuWatchdogThread* watchdog_thread,
    const gpu::GPUInfo* gpu_info,
    const gpu::GpuPreferences& gpu_prefs) {
  // TODO(sad): https://crbug.com/645602
  return true;
}

}  // namespace viz
