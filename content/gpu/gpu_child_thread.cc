// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_child_thread.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/common/field_trial_recorder.mojom.h"
#include "content/gpu/gpu_service_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/gpu/content_gpu_client.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/ui/gpu/interfaces/gpu_service.mojom.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_ANDROID)
#include "media/base/android/media_drm_bridge_client.h"
#endif

namespace content {
namespace {

ChildThreadImpl::Options GetOptions() {
  ChildThreadImpl::Options::Builder builder;

#if defined(USE_OZONE)
  IPC::MessageFilter* message_filter =
      ui::OzonePlatform::GetInstance()->GetGpuMessageFilter();
  if (message_filter)
    builder.AddStartupFilter(message_filter);
#endif

  builder.ConnectToBrowser(true);

  return builder.Build();
}

}  // namespace

GpuChildThread::GpuChildThread(
    std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread,
    bool dead_on_arrival,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    DeferredMessages deferred_messages)
    : GpuChildThread(GetOptions(),
                     std::move(watchdog_thread),
                     dead_on_arrival,
                     false /* in_browser_process */,
                     gpu_info,
                     gpu_feature_info) {
  deferred_messages_ = std::move(deferred_messages);
}

GpuChildThread::GpuChildThread(const InProcessChildThreadParams& params,
                               const gpu::GPUInfo& gpu_info,
                               const gpu::GpuFeatureInfo& gpu_feature_info)
    : GpuChildThread(ChildThreadImpl::Options::Builder()
                         .InBrowserProcess(params)
                         .ConnectToBrowser(true)
                         .Build(),
                     nullptr /* watchdog_thread */,
                     false /* dead_on_arrival */,
                     true /* in_browser_process */,
                     gpu_info,
                     gpu_feature_info) {}

GpuChildThread::GpuChildThread(
    const ChildThreadImpl::Options& options,
    std::unique_ptr<gpu::GpuWatchdogThread> gpu_watchdog_thread,
    bool dead_on_arrival,
    bool in_browser_process,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info)
    : ChildThreadImpl(options),
      dead_on_arrival_(dead_on_arrival),
      in_browser_process_(in_browser_process),
      gpu_service_(new ui::GpuService(gpu_info,
                                      std::move(gpu_watchdog_thread),
                                      ChildProcess::current()->io_task_runner(),
                                      gpu_feature_info)),
      gpu_main_binding_(this) {
  if (in_browser_process_) {
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kSingleProcess) ||
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kInProcessGPU));
  }
  gpu_service_->set_in_host_process(in_browser_process_);
}

GpuChildThread::~GpuChildThread() {
}

void GpuChildThread::Init(const base::Time& process_start_time) {
  gpu_service_->set_start_time(process_start_time);

#if defined(OS_ANDROID)
  // When running in in-process mode, this has been set in the browser at
  // ChromeBrowserMainPartsAndroid::PreMainMessageLoopRun().
  if (!in_browser_process_)
    media::SetMediaDrmBridgeClient(
        GetContentClient()->GetMediaDrmBridgeClient());
#endif
  // We don't want to process any incoming interface requests until
  // OnInitialize() is invoked.
  GetInterfaceRegistry()->PauseBinding();

  if (GetContentClient()->gpu())  // NULL in tests.
    GetContentClient()->gpu()->Initialize(this);
  AssociatedInterfaceRegistry* registry = &associated_interfaces_;
  registry->AddInterface(base::Bind(
      &GpuChildThread::CreateGpuMainService, base::Unretained(this)));
}

void GpuChildThread::OnFieldTrialGroupFinalized(const std::string& trial_name,
                                                const std::string& group_name) {
  mojom::FieldTrialRecorderPtr field_trial_recorder;
  GetConnector()->BindInterface(mojom::kBrowserServiceName,
                                &field_trial_recorder);
  field_trial_recorder->FieldTrialActivated(trial_name);
}

void GpuChildThread::CreateGpuMainService(
    ui::mojom::GpuMainAssociatedRequest request) {
  gpu_main_binding_.Bind(std::move(request));
}

bool GpuChildThread::Send(IPC::Message* msg) {
  // The GPU process must never send a synchronous IPC message to the browser
  // process. This could result in deadlock.
  DCHECK(!msg->is_sync());

  return ChildThreadImpl::Send(msg);
}

void GpuChildThread::OnAssociatedInterfaceRequest(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (associated_interfaces_.CanBindRequest(name))
    associated_interfaces_.BindRequest(name, std::move(handle));
  else
    ChildThreadImpl::OnAssociatedInterfaceRequest(name, std::move(handle));
}

void GpuChildThread::CreateGpuService(
    ui::mojom::GpuServiceRequest request,
    ui::mojom::GpuHostPtr gpu_host,
    const gpu::GpuPreferences& gpu_preferences,
    mojo::ScopedSharedBufferHandle activity_flags) {
  gpu_service_->UpdateGPUInfoFromPreferences(gpu_preferences);
  for (const LogMessage& log : deferred_messages_)
    gpu_host->RecordLogMessage(log.severity, log.header, log.message);
  deferred_messages_.clear();

  if (dead_on_arrival_) {
    LOG(ERROR) << "Exiting GPU process due to errors during initialization";
    gpu_service_.reset();
    gpu_host->DidFailInitialize();
    base::MessageLoop::current()->QuitWhenIdle();
    return;
  }

  // Bind should happen only if initialization succeeds (i.e. not dead on
  // arrival), because otherwise, it can receive requests from the host while in
  // an uninitialized state.
  gpu_service_->Bind(std::move(request));
  gpu::SyncPointManager* sync_point_manager = nullptr;
  // Note SyncPointManager from ContentGpuClient cannot be owned by this.
  if (GetContentClient()->gpu())
    sync_point_manager = GetContentClient()->gpu()->GetSyncPointManager();
  gpu_service_->InitializeWithHost(
      std::move(gpu_host),
      gpu::GpuProcessActivityFlags(std::move(activity_flags)),
      sync_point_manager, ChildProcess::current()->GetShutDownEvent());
  CHECK(gpu_service_->media_gpu_channel_manager());

  // Only set once per process instance.
  service_factory_.reset(new GpuServiceFactory(
      gpu_service_->media_gpu_channel_manager()->AsWeakPtr()));

  GetInterfaceRegistry()->AddInterface(base::Bind(
      &GpuChildThread::BindServiceFactoryRequest, base::Unretained(this)));

  if (GetContentClient()->gpu()) {  // NULL in tests.
    GetContentClient()->gpu()->ExposeInterfacesToBrowser(GetInterfaceRegistry(),
                                                         gpu_preferences);
    GetContentClient()->gpu()->ConsumeInterfacesFromBrowser(GetConnector());
  }

  GetInterfaceRegistry()->ResumeBinding();
}

void GpuChildThread::CreateFrameSinkManager(
    cc::mojom::FrameSinkManagerRequest request,
    cc::mojom::FrameSinkManagerClientPtr client) {
  NOTREACHED();
}

void GpuChildThread::BindServiceFactoryRequest(
    service_manager::mojom::ServiceFactoryRequest request) {
  DVLOG(1) << "GPU: Binding service_manager::mojom::ServiceFactoryRequest";
  DCHECK(service_factory_);
  service_factory_bindings_.AddBinding(service_factory_.get(),
                                       std::move(request));
}

}  // namespace content
