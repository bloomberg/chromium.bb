// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_child_thread.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/gpu/gpu_service_factory.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/gpu/content_gpu_client.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/viz/gl/privileged/interfaces/gpu_service.mojom.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_ANDROID)
#include "media/base/android/media_drm_bridge_client.h"
#include "media/mojo/clients/mojo_android_overlay.h"
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

  builder.AutoStartServiceManagerConnection(false);
  builder.ConnectToBrowser(true);

  return builder.Build();
}

// This ConnectionFilter queues all incoming bind interface requests until
// Release() is called.
class QueueingConnectionFilter : public ConnectionFilter {
 public:
  QueueingConnectionFilter(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      std::unique_ptr<service_manager::BinderRegistry> registry)
      : io_task_runner_(io_task_runner),
        registry_(std::move(registry)),
        weak_factory_(this) {
    // This will be reattached by any of the IO thread functions on first call.
    io_thread_checker_.DetachFromThread();
  }
  ~QueueingConnectionFilter() override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
  }

  base::Closure GetReleaseCallback() {
    return base::Bind(base::IgnoreResult(&base::TaskRunner::PostTask),
                      io_task_runner_, FROM_HERE,
                      base::Bind(&QueueingConnectionFilter::Release,
                                 weak_factory_.GetWeakPtr()));
  }

 private:
  struct PendingRequest {
    std::string interface_name;
    mojo::ScopedMessagePipeHandle interface_pipe;
  };

  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    if (registry_->CanBindInterface(interface_name)) {
      if (released_) {
        registry_->BindInterface(interface_name, std::move(*interface_pipe));
      } else {
        std::unique_ptr<PendingRequest> request =
            base::MakeUnique<PendingRequest>();
        request->interface_name = interface_name;
        request->interface_pipe = std::move(*interface_pipe);
        pending_requests_.push_back(std::move(request));
      }
    }
  }

  void Release() {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    released_ = true;
    for (auto& request : pending_requests_) {
      registry_->BindInterface(request->interface_name,
                               std::move(request->interface_pipe));
    }
  }

  base::ThreadChecker io_thread_checker_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  bool released_ = false;
  std::vector<std::unique_ptr<PendingRequest>> pending_requests_;
  std::unique_ptr<service_manager::BinderRegistry> registry_;

  base::WeakPtrFactory<QueueingConnectionFilter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QueueingConnectionFilter);
};

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
                         .AutoStartServiceManagerConnection(false)
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
      gpu_service_(
          new viz::GpuServiceImpl(gpu_info,
                                  std::move(gpu_watchdog_thread),
                                  ChildProcess::current()->io_task_runner(),
                                  gpu_feature_info)),
      gpu_main_binding_(this),
      weak_factory_(this) {
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
  AssociatedInterfaceRegistry* associated_registry = &associated_interfaces_;
  associated_registry->AddInterface(base::Bind(
      &GpuChildThread::CreateGpuMainService, base::Unretained(this)));

  auto registry = base::MakeUnique<service_manager::BinderRegistry>();
  registry->AddInterface(base::Bind(&GpuChildThread::BindServiceFactoryRequest,
                                    weak_factory_.GetWeakPtr()),
                         base::ThreadTaskRunnerHandle::Get());
  if (GetContentClient()->gpu())  // NULL in tests.
    GetContentClient()->gpu()->Initialize(registry.get());

  std::unique_ptr<QueueingConnectionFilter> filter =
      base::MakeUnique<QueueingConnectionFilter>(GetIOTaskRunner(),
                                                 std::move(registry));
  release_pending_requests_closure_ = filter->GetReleaseCallback();
  GetServiceManagerConnection()->AddConnectionFilter(std::move(filter));

  StartServiceManagerConnection();
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
    viz::mojom::GpuServiceRequest request,
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
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
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

#if defined(OS_ANDROID)
  gpu_service_->media_gpu_channel_manager()->SetOverlayFactory(
      base::Bind(&GpuChildThread::CreateAndroidOverlay));
#endif

  if (GetContentClient()->gpu())  // NULL in tests.
    GetContentClient()->gpu()->GpuServiceInitialized(gpu_preferences);

  release_pending_requests_closure_.Run();
}

void GpuChildThread::CreateFrameSinkManager(
    viz::mojom::FrameSinkManagerRequest request,
    viz::mojom::FrameSinkManagerClientPtr client) {
  NOTREACHED();
}

void GpuChildThread::BindServiceFactoryRequest(
    service_manager::mojom::ServiceFactoryRequest request) {
  DVLOG(1) << "GPU: Binding service_manager::mojom::ServiceFactoryRequest";
  DCHECK(service_factory_);
  service_factory_bindings_.AddBinding(service_factory_.get(),
                                       std::move(request));
}

#if defined(OS_ANDROID)
// static
std::unique_ptr<media::AndroidOverlay> GpuChildThread::CreateAndroidOverlay(
    const base::UnguessableToken& routing_token,
    media::AndroidOverlayConfig config) {
  media::mojom::AndroidOverlayProviderPtr provider_ptr;
  ChildThread::Get()->GetConnector()->BindInterface(
      content::mojom::kBrowserServiceName, &provider_ptr);
  return base::MakeUnique<media::MojoAndroidOverlay>(
      std::move(provider_ptr), std::move(config), routing_token);
}
#endif

}  // namespace content
