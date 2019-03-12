// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_manager/service_manager_context.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/service_manager/common_browser_interfaces.h"
#include "content/browser/utility_process_host.h"
#include "content/browser/utility_process_host_client.h"
#include "content/browser/wake_lock/wake_lock_context_host.h"
#include "content/common/service_manager/service_manager_connection_impl.h"
#include "content/public/app/content_browser_manifest.h"
#include "content/public/app/content_gpu_manifest.h"
#include "content/public/app/content_packaged_services_manifest.h"
#include "content/public/app/content_plugin_manifest.h"
#include "content/public/app/content_renderer_manifest.h"
#include "content/public/app/content_utility_manifest.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_service_registry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "media/audio/audio_manager.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/interfaces/constants.mojom.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/service_factory.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/device/device_service.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/media_session/media_session_service.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/metrics/metrics_mojo_service.h"
#include "services/metrics/public/mojom/constants.mojom.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/cross_thread_shared_url_loader_factory_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/resource_coordinator/resource_coordinator_service.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/service_manager.h"
#include "services/service_manager/service_process_launcher.h"
#include "services/shape_detection/public/mojom/constants.mojom.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/tracing_service.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/service_impl.h"
#include "services/viz/public/interfaces/constants.mojom.h"
#include "ui/base/buildflags.h"
#include "ui/base/ui_base_features.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "jni/ContentNfcDelegate_jni.h"
#endif

#if defined(OS_LINUX)
#include "components/services/font/font_service_app.h"
#include "components/services/font/public/interfaces/constants.mojom.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/assistant/buildflags.h"  // nogncheck
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif

namespace content {

namespace {

base::LazyInstance<std::unique_ptr<service_manager::Connector>>::Leaky
    g_io_thread_connector = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<std::map<std::string, base::WeakPtr<UtilityProcessHost>>>::
    Leaky g_active_process_groups;

// If enabled, network service will run in it's own thread when running
// in-process, otherwise it is run on the IO thread.
// On ChromeOS the network service has to run on the IO thread because
// ProfileIOData and NetworkContext both try to set up NSS, which has has to be
// called from the IO thread.
const base::Feature kNetworkServiceDedicatedThread{
  "NetworkServiceDedicatedThread",
#if defined(OS_CHROMEOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

void DestroyConnectorOnIOThread() { g_io_thread_connector.Get().reset(); }

// Launch a process for a service once its sandbox type is known.
void StartServiceInUtilityProcess(
    const std::string& service_name,
    const ContentBrowserClient::ProcessNameCallback& process_name_callback,
    base::Optional<std::string> process_group,
    service_manager::mojom::ServiceRequest request,
    service_manager::mojom::PIDReceiverPtr pid_receiver,
    service_manager::mojom::ServiceInfoPtr service_info) {
  DCHECK(service_info);
  service_manager::SandboxType sandbox_type =
      service_manager::UtilitySandboxTypeFromString(service_info->sandbox_type);

  // Look for an existing process group.
  base::WeakPtr<UtilityProcessHost>* weak_host = nullptr;
  if (process_group)
    weak_host = &g_active_process_groups.Get()[*process_group];

  UtilityProcessHost* process_host = nullptr;
  if (weak_host && *weak_host) {
    // Start service in an existing process.
    process_host = weak_host->get();
  } else {
    // Start a new process for this service.
    UtilityProcessHost* impl = new UtilityProcessHost(nullptr, nullptr);
    base::string16 process_name = process_name_callback.Run();
    DCHECK(!process_name.empty());
    impl->SetName(process_name);
    impl->SetMetricsName(service_name);
    // NOTE: This is not the service instance's real Identity. For all current
    // practical purposes however, only the name is relevant.
    impl->SetServiceIdentity(service_manager::Identity(
        service_name, service_manager::kSystemInstanceGroup, base::Token{},
        base::Token{1, 1}));
    impl->SetSandboxType(sandbox_type);
    impl->Start();
    impl->SetLaunchCallback(
        base::BindOnce([](service_manager::mojom::PIDReceiverPtr pid_receiver,
                          base::ProcessId pid) { pid_receiver->SetPID(pid); },
                       std::move(pid_receiver)));
    if (weak_host)
      *weak_host = impl->AsWeakPtr();
    process_host = impl;
  }

  process_host->RunService(
      service_name, mojo::PendingReceiver<service_manager::mojom::Service>(
                        request.PassMessagePipe()));
}

// Determine a sandbox type for a service and launch a process for it.
void QueryAndStartServiceInUtilityProcess(
    const std::string& service_name,
    const ContentBrowserClient::ProcessNameCallback& process_name_callback,
    base::Optional<std::string> process_group,
    service_manager::mojom::ServiceRequest request,
    service_manager::mojom::PIDReceiverPtr pid_receiver) {
  ServiceManagerContext::GetConnectorForIOThread()->QueryService(
      service_name,
      base::BindOnce(&StartServiceInUtilityProcess, service_name,
                     process_name_callback, std::move(process_group),
                     std::move(request), std::move(pid_receiver)));
}

// Send a RunService request through the GpuProcessHost.
void StartServiceInGpuProcess(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request,
    service_manager::mojom::PIDReceiverPtr pid_receiver) {
  GpuProcessHost* process_host = GpuProcessHost::Get();
  if (!process_host) {
    DLOG(ERROR) << "GPU process host not available.";
    return;
  }

  // TODO(xhwang): It's possible that |process_host| is non-null, but the actual
  // process is dead. In that case, |request| will be dropped and application
  // load requests through ServiceFactory will also fail. Make sure we handle
  // these cases correctly.
  process_host->gpu_host()->RunService(
      service_name, mojo::PendingReceiver<service_manager::mojom::Service>(
                        request.PassMessagePipe()));
}

class NullServiceProcessLauncherFactory
    : public service_manager::ServiceProcessLauncherFactory {
 public:
  NullServiceProcessLauncherFactory() {}
  ~NullServiceProcessLauncherFactory() override {}

 private:
  std::unique_ptr<service_manager::ServiceProcessLauncher> Create(
      const base::FilePath& service_path) override {
    // There are innocuous races where browser code may attempt to connect
    // to a specific renderer instance through the Service Manager after that
    // renderer has been terminated. These result in this code path being hit
    // fairly regularly and the resulting log spam causes confusion. We suppress
    // this message only for "content_renderer".
    const base::FilePath::StringType kRendererServiceFilename =
        base::FilePath().AppendASCII(mojom::kRendererServiceName).value();
    const base::FilePath::StringType service_executable =
        service_path.BaseName().value();
    if (service_executable.find(kRendererServiceFilename) ==
        base::FilePath::StringType::npos) {
      LOG(ERROR) << "Attempting to run unsupported native service: "
                 << service_path.value();
    }
    return nullptr;
  }

  DISALLOW_COPY_AND_ASSIGN(NullServiceProcessLauncherFactory);
};

// This class is intended for tests that want to load service binaries (rather
// than via the utility process). Production code uses
// NullServiceProcessLauncherFactory.
class ServiceBinaryLauncherFactory
    : public service_manager::ServiceProcessLauncherFactory {
 public:
  ServiceBinaryLauncherFactory() = default;
  ~ServiceBinaryLauncherFactory() override = default;

 private:
  std::unique_ptr<service_manager::ServiceProcessLauncher> Create(
      const base::FilePath& service_path) override {
    return std::make_unique<service_manager::ServiceProcessLauncher>(
        nullptr, service_path);
  }

  DISALLOW_COPY_AND_ASSIGN(ServiceBinaryLauncherFactory);
};

// SharedURLLoaderFactory for device service, backed by
// GetContentClient()->browser()->GetSystemSharedURLLoaderFactory().
class DeviceServiceURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  DeviceServiceURLLoaderFactory() = default;

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    GetContentClient()
        ->browser()
        ->GetSystemSharedURLLoaderFactory()
        ->CreateLoaderAndStart(std::move(request), routing_id, request_id,
                               options, url_request, std::move(client),
                               traffic_annotation);
  }

  // SharedURLLoaderFactory implementation:
  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    GetContentClient()->browser()->GetSystemSharedURLLoaderFactory()->Clone(
        std::move(request));
  }

  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    return std::make_unique<network::CrossThreadSharedURLLoaderFactoryInfo>(
        this);
  }

 private:
  friend class base::RefCounted<DeviceServiceURLLoaderFactory>;
  ~DeviceServiceURLLoaderFactory() override = default;

  DISALLOW_COPY_AND_ASSIGN(DeviceServiceURLLoaderFactory);
};

std::unique_ptr<service_manager::Service> CreateNetworkService(
    service_manager::mojom::ServiceRequest service_request) {
  // The test interface doesn't need to be implemented in the in-process case.
  auto registry = std::make_unique<service_manager::BinderRegistry>();
  registry->AddInterface(base::BindRepeating(
      [](network::mojom::NetworkServiceTestRequest request) {}));
  return std::make_unique<network::NetworkService>(
      std::move(registry), nullptr /* request */, nullptr /* net_log */,
      std::move(service_request));
}

bool AudioServiceOutOfProcess() {
  // Returns true iff kAudioServiceOutOfProcess feature is enabled and if the
  // embedder does not provide its own in-process AudioManager.
  return base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) &&
         !GetContentClient()->browser()->OverridesAudioManager();
}

using InProcessServiceFactory =
    base::RepeatingCallback<std::unique_ptr<service_manager::Service>(
        service_manager::mojom::ServiceRequest request)>;

void LaunchInProcessServiceOnSequence(
    const InProcessServiceFactory& factory,
    service_manager::mojom::ServiceRequest request) {
  service_manager::Service::RunAsyncUntilTermination(
      factory.Run(std::move(request)));
}

void LaunchInProcessService(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const InProcessServiceFactory& factory,
    service_manager::mojom::ServiceRequest request) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&LaunchInProcessServiceOnSequence, factory,
                                std::move(request)));
}

void RegisterInProcessService(
    ServiceManagerConnection* connection,
    const std::string& service_name,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const InProcessServiceFactory& factory) {
  connection->AddServiceRequestHandler(
      service_name,
      base::BindRepeating(&LaunchInProcessService, task_runner, factory));
}

std::unique_ptr<service_manager::Service> CreateVideoCaptureService(
    service_manager::mojom::ServiceRequest request) {
  return std::make_unique<video_capture::ServiceImpl>(
      std::move(request), base::CreateSingleThreadTaskRunnerWithTraits(
                              {content::BrowserThread::UI}));
}

void CreateInProcessAudioService(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    service_manager::mojom::ServiceRequest request) {
  // TODO(https://crbug.com/853254): Remove BrowserMainLoop::GetAudioManager().
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](media::AudioManager* audio_manager,
                        service_manager::mojom::ServiceRequest request) {
                       service_manager::Service::RunAsyncUntilTermination(
                           audio::CreateEmbeddedService(audio_manager,
                                                        std::move(request)));
                     },
                     BrowserMainLoop::GetAudioManager(), std::move(request)));
}

#if defined(OS_LINUX)
std::unique_ptr<service_manager::Service> CreateFontService(
    service_manager::mojom::ServiceRequest request) {
  return std::make_unique<font_service::FontServiceApp>(std::move(request));
}
#endif  // defined(OS_LINUX)

std::unique_ptr<service_manager::Service> CreateResourceCoordinatorService(
    service_manager::mojom::ServiceRequest request) {
  return std::make_unique<resource_coordinator::ResourceCoordinatorService>(
      std::move(request));
}

std::unique_ptr<service_manager::Service> CreateTracingService(
    service_manager::mojom::ServiceRequest request) {
  return std::make_unique<tracing::TracingService>(std::move(request));
}

std::unique_ptr<service_manager::Service> CreateMediaSessionService(
    service_manager::mojom::ServiceRequest request) {
  return std::make_unique<media_session::MediaSessionService>(
      std::move(request));
}

}  // namespace

// State which lives on the IO thread and drives the ServiceManager.
class ServiceManagerContext::InProcessServiceManagerContext
    : public base::RefCountedThreadSafe<InProcessServiceManagerContext> {
 public:
  InProcessServiceManagerContext(scoped_refptr<base::SingleThreadTaskRunner>
                                     service_manager_thread_task_runner)
      : service_manager_thread_task_runner_(
            service_manager_thread_task_runner) {}

  void Start(
      service_manager::mojom::ServicePtrInfo packaged_services_service_info,
      std::vector<service_manager::Manifest> manifests) {
    service_manager_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InProcessServiceManagerContext::StartOnServiceManagerThread, this,
            std::move(manifests), std::move(packaged_services_service_info),
            base::ThreadTaskRunnerHandle::Get()));
  }

  void ShutDown() {
    service_manager_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InProcessServiceManagerContext::ShutDownOnServiceManagerThread,
            this));
  }

  void StartServices(std::vector<std::string> service_names) {
    service_manager_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&InProcessServiceManagerContext ::
                                      StartServicesOnServiceManagerThread,
                                  this, std::move(service_names)));
  }

 private:
  friend class base::RefCountedThreadSafe<InProcessServiceManagerContext>;

  ~InProcessServiceManagerContext() {}

  void StartOnServiceManagerThread(
      std::vector<service_manager::Manifest> manifests,
      service_manager::mojom::ServicePtrInfo packaged_services_service_info,
      scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner) {
    std::unique_ptr<service_manager::ServiceProcessLauncherFactory>
        service_process_launcher_factory;
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableServiceBinaryLauncher)) {
      service_process_launcher_factory =
          std::make_unique<ServiceBinaryLauncherFactory>();
    } else {
      service_process_launcher_factory =
          std::make_unique<NullServiceProcessLauncherFactory>();
    }
    service_manager_ = std::make_unique<service_manager::ServiceManager>(
        std::move(service_process_launcher_factory), std::move(manifests));

    service_manager::mojom::ServicePtr packaged_services_service;
    packaged_services_service.Bind(std::move(packaged_services_service_info));
    service_manager_->RegisterService(
        service_manager::Identity(mojom::kPackagedServicesServiceName,
                                  service_manager::kSystemInstanceGroup,
                                  base::Token{}, base::Token::CreateRandom()),
        std::move(packaged_services_service), nullptr);
    service_manager_->SetInstanceQuitCallback(
        base::Bind(&OnInstanceQuitOnServiceManagerThread,
                   std::move(ui_thread_task_runner)));
  }

  static void OnInstanceQuitOnServiceManagerThread(
      scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
      const service_manager::Identity& id) {
    ui_thread_task_runner->PostTask(FROM_HERE,
                                    base::BindOnce(&OnInstanceQuit, id));
  }

  static void OnInstanceQuit(const service_manager::Identity& id) {
    if (GetContentClient()->browser()->ShouldTerminateOnServiceQuit(id)) {
      // Don't LOG(FATAL) because we don't want a browser crash report.
      LOG(ERROR) << "Terminating because service '" << id.name()
                 << "' quit unexpectedly.";
      // Skip shutdown to reduce the risk that other code in the browser will
      // respond to the service pipe closing.
      exit(1);
    }
  }

  void ShutDownOnServiceManagerThread() {
    service_manager_.reset();
  }

  void StartServicesOnServiceManagerThread(
      std::vector<std::string> service_names) {
    if (!service_manager_)
      return;

    for (const auto& service_name : service_names)
      service_manager_->StartService(service_name);
  }

  scoped_refptr<base::SingleThreadTaskRunner>
      service_manager_thread_task_runner_;
  std::unique_ptr<service_manager::ServiceManager> service_manager_;

  DISALLOW_COPY_AND_ASSIGN(InProcessServiceManagerContext);
};

ServiceManagerContext::ServiceManagerContext(
    scoped_refptr<base::SingleThreadTaskRunner>
        service_manager_thread_task_runner)
    : service_manager_thread_task_runner_(
          std::move(service_manager_thread_task_runner)) {
  // The |service_manager_thread_task_runner_| must have been created before
  // starting the ServiceManager.
  DCHECK(service_manager_thread_task_runner_);
  std::vector<service_manager::Manifest> manifests{
      GetContentBrowserManifest(),          GetContentGpuManifest(),
      GetContentPackagedServicesManifest(), GetContentPluginManifest(),
      GetContentRendererManifest(),         GetContentUtilityManifest(),
  };
  for (auto& manifest : manifests) {
    base::Optional<service_manager::Manifest> overlay =
        GetContentClient()->browser()->GetServiceManifestOverlay(
            manifest.service_name);
    if (overlay)
      manifest.Amend(*overlay);
    if (!manifest.preloaded_files.empty()) {
      std::map<std::string, base::FilePath> preloaded_files_map;
      for (const auto& info : manifest.preloaded_files)
        preloaded_files_map.emplace(info.key, info.path);
      ChildProcessLauncher::SetRegisteredFilesForService(
          manifest.service_name, std::move(preloaded_files_map));
    }
  }
  for (auto& extra_manifest :
       GetContentClient()->browser()->GetExtraServiceManifests()) {
    manifests.emplace_back(std::move(extra_manifest));
  }
  in_process_context_ =
      new InProcessServiceManagerContext(service_manager_thread_task_runner_);

  service_manager::mojom::ServicePtr packaged_services_service;
  service_manager::mojom::ServiceRequest packaged_services_request =
      mojo::MakeRequest(&packaged_services_service);
  in_process_context_->Start(packaged_services_service.PassInterface(),
                             std::move(manifests));

  packaged_services_connection_ =
      ServiceManagerConnection::Create(std::move(packaged_services_request),
                                       service_manager_thread_task_runner_);
  packaged_services_connection_->SetDefaultServiceRequestHandler(
      base::BindRepeating(&ServiceManagerContext::OnUnhandledServiceRequest,
                          weak_ptr_factory_.GetWeakPtr()));

  service_manager::mojom::ServicePtr root_browser_service;
  ServiceManagerConnection::SetForProcess(
      ServiceManagerConnection::Create(mojo::MakeRequest(&root_browser_service),
                                       service_manager_thread_task_runner_));
  auto* browser_connection = ServiceManagerConnection::GetForProcess();

  service_manager::mojom::PIDReceiverPtr pid_receiver;
  packaged_services_connection_->GetConnector()->RegisterServiceInstance(
      service_manager::Identity(mojom::kBrowserServiceName,
                                service_manager::kSystemInstanceGroup,
                                base::Token{}, base::Token::CreateRandom()),
      std::move(root_browser_service), mojo::MakeRequest(&pid_receiver));
  pid_receiver->SetPID(base::GetCurrentProcId());

  RegisterInProcessService(
      packaged_services_connection_.get(),
      resource_coordinator::mojom::kServiceName,
      service_manager_thread_task_runner_,
      base::BindRepeating(&CreateResourceCoordinatorService));

  RegisterInProcessService(packaged_services_connection_.get(),
                           metrics::mojom::kMetricsServiceName,
                           service_manager_thread_task_runner_,
                           base::BindRepeating(&metrics::CreateMetricsService));

  if (base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    RegisterInProcessService(packaged_services_connection_.get(),
                             media_session::mojom::kServiceName,
                             base::SequencedTaskRunnerHandle::Get(),
                             base::BindRepeating(&CreateMediaSessionService));
  }

  if (features::IsVideoCaptureServiceEnabledForBrowserProcess()) {
    RegisterInProcessService(
        packaged_services_connection_.get(), video_capture::mojom::kServiceName,
#if defined(OS_WIN)
        base::CreateCOMSTATaskRunnerWithTraits(
#else
        base::CreateSingleThreadTaskRunnerWithTraits(
#endif
            base::TaskTraits({base::MayBlock(), base::WithBaseSyncPrimitives(),
                              base::TaskPriority::BEST_EFFORT}),
            base::SingleThreadTaskRunnerThreadMode::DEDICATED),
        base::BindRepeating(&CreateVideoCaptureService));
  }

#if defined(OS_LINUX)
  RegisterInProcessService(
      packaged_services_connection_.get(), font_service::mojom::kServiceName,
      base::CreateSequencedTaskRunnerWithTraits(
          base::TaskTraits({base::MayBlock(), base::WithBaseSyncPrimitives(),
                            base::TaskPriority::USER_BLOCKING})),
      base::BindRepeating(&CreateFontService));
#endif

  GetContentClient()->browser()->RegisterIOThreadServiceHandlers(
      packaged_services_connection_.get());

  // This is safe to assign directly from any thread, because
  // ServiceManagerContext must be constructed before anyone can call
  // GetConnectorForIOThread().
  g_io_thread_connector.Get() = browser_connection->GetConnector()->Clone();

  ContentBrowserClient::OutOfProcessServiceMap out_of_process_services;
  GetContentClient()->browser()->RegisterOutOfProcessServices(
      &out_of_process_services);

  if (base::FeatureList::IsEnabled(features::kTracingServiceInProcess)) {
    RegisterInProcessService(
        packaged_services_connection_.get(), tracing::mojom::kServiceName,
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
             base::WithBaseSyncPrimitives(),
             base::TaskPriority::USER_BLOCKING}),
        base::BindRepeating(&CreateTracingService));
  } else {
    out_of_process_services[tracing::mojom::kServiceName] =
        base::BindRepeating(&base::ASCIIToUTF16, "Tracing Service");
  }

  out_of_process_services[data_decoder::mojom::kServiceName] =
      base::BindRepeating(&base::ASCIIToUTF16, "Data Decoder Service");

  bool network_service_enabled =
      base::FeatureList::IsEnabled(network::features::kNetworkService);
  if (network_service_enabled) {
    if (IsInProcessNetworkService()) {
      scoped_refptr<base::SequencedTaskRunner> task_runner =
          service_manager_thread_task_runner_;
      if (base::FeatureList::IsEnabled(kNetworkServiceDedicatedThread)) {
        base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
        network_service_thread_.StartWithOptions(options);
        task_runner = network_service_thread_.task_runner();
      }

      GetNetworkTaskRunner()->StartWithTaskRunner(task_runner);
      RegisterInProcessService(packaged_services_connection_.get(),
                               mojom::kNetworkServiceName, task_runner,
                               base::BindRepeating(&CreateNetworkService));
    } else {
      out_of_process_services[mojom::kNetworkServiceName] =
          base::BindRepeating(&base::ASCIIToUTF16, "Network Service");
    }
  }

  if (AudioServiceOutOfProcess()) {
    DCHECK(base::FeatureList::IsEnabled(features::kAudioServiceAudioStreams));
    out_of_process_services[audio::mojom::kServiceName] =
        base::BindRepeating(&base::ASCIIToUTF16, "Audio Service");
  } else {
    packaged_services_connection_->AddServiceRequestHandler(
        audio::mojom::kServiceName,
        base::BindRepeating(&CreateInProcessAudioService,
                            base::WrapRefCounted(GetAudioServiceRunner())));
  }

  if (features::IsVideoCaptureServiceEnabledForOutOfProcess()) {
    out_of_process_services[video_capture::mojom::kServiceName] =
        base::BindRepeating(&base::ASCIIToUTF16, "Video Capture Service");
  }

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
  out_of_process_services[media::mojom::kMediaServiceName] =
      base::BindRepeating(&base::ASCIIToUTF16, "Media Service");
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  out_of_process_services[media::mojom::kCdmServiceName] = base::BindRepeating(
      &base::ASCIIToUTF16, "Content Decryption Module Service");
#endif

#if defined(OS_CHROMEOS)
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  out_of_process_services
      [chromeos::assistant::mojom::kAudioDecoderServiceName] =
          base::BindRepeating(&base::ASCIIToUTF16,
                              "Assistant Audio Decoder Service");
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif

  if (features::IsMultiProcessMash() && features::IsMashOopVizEnabled()) {
    out_of_process_services[viz::mojom::kVizServiceName] =
        base::BindRepeating(&base::ASCIIToUTF16, "Visuals Service");
  }

  for (const auto& service : out_of_process_services) {
    packaged_services_connection_->AddServiceRequestHandlerWithPID(
        service.first,
        base::BindRepeating(&QueryAndStartServiceInUtilityProcess,
                            service.first, service.second.process_name_callback,
                            service.second.process_group));
  }

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  packaged_services_connection_->AddServiceRequestHandlerWithPID(
      media::mojom::kMediaServiceName,
      base::Bind(&StartServiceInGpuProcess, media::mojom::kMediaServiceName));
#endif

  packaged_services_connection_->AddServiceRequestHandlerWithPID(
      shape_detection::mojom::kServiceName,
      base::Bind(&StartServiceInGpuProcess,
                 shape_detection::mojom::kServiceName));

  packaged_services_connection_->Start();

  in_process_context_->StartServices(
      GetContentClient()->browser()->GetStartupServices());
}

ServiceManagerContext::~ServiceManagerContext() {
  ShutDown();
}

void ServiceManagerContext::ShutDown() {
  // NOTE: The in-process ServiceManager MUST be destroyed before the browser
  // process-wide ServiceManagerConnection. Otherwise it's possible for the
  // ServiceManager to receive connection requests for service:content_browser
  // which it may attempt to service by launching a new instance of the browser.
  if (in_process_context_)
    in_process_context_->ShutDown();
  if (ServiceManagerConnection::GetForProcess())
    ServiceManagerConnection::DestroyForProcess();
  service_manager_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DestroyConnectorOnIOThread));
  packaged_services_connection_.reset();
}

// static
service_manager::Connector* ServiceManagerContext::GetConnectorForIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return g_io_thread_connector.Get().get();
}

// static
bool ServiceManagerContext::HasValidProcessForProcessGroup(
    const std::string& process_group_name) {
  auto iter = g_active_process_groups.Get().find(process_group_name);
  if (iter == g_active_process_groups.Get().end() || !iter->second)
    return false;
  return iter->second->GetData().GetProcess().IsValid();
}

// static
void ServiceManagerContext::StartBrowserConnection() {
  auto* browser_connection = ServiceManagerConnection::GetForProcess();
  RegisterCommonBrowserInterfaces(browser_connection);
  browser_connection->Start();

  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  // Create the in-process NetworkService object so that its getter is
  // available on the IO thread.
  GetNetworkService();
}

// static
base::DeferredSequencedTaskRunner*
ServiceManagerContext::GetAudioServiceRunner() {
  static base::NoDestructor<scoped_refptr<base::DeferredSequencedTaskRunner>>
      instance(new base::DeferredSequencedTaskRunner);
  return (*instance).get();
}

void ServiceManagerContext::OnUnhandledServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (service_name == device::mojom::kServiceName) {
    // This task runner may be used by some device service implementation bits
    // to interface with dbus client code, which in turn imposes some subtle
    // thread affinity on the clients. We therefore require a single-thread
    // runner.
    scoped_refptr<base::SingleThreadTaskRunner> device_blocking_task_runner =
        base::CreateSingleThreadTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
#if defined(OS_ANDROID)
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaGlobalRef<jobject> java_nfc_delegate;
    java_nfc_delegate.Reset(Java_ContentNfcDelegate_create(env));
    DCHECK(!java_nfc_delegate.is_null());

    // See the comments on wake_lock_context_host.h, content_browser_client.h
    // and ContentNfcDelegate.java respectively for comments on those
    // parameters.
    auto service = device::CreateDeviceService(
        device_blocking_task_runner, service_manager_thread_task_runner_,
        base::MakeRefCounted<DeviceServiceURLLoaderFactory>(),
        GetContentClient()->browser()->GetGeolocationApiKey(),
        GetContentClient()->browser()->ShouldUseGmsCoreGeolocationProvider(),
        base::BindRepeating(&WakeLockContextHost::GetNativeViewForContext),
        base::BindRepeating(
            &ContentBrowserClient::OverrideSystemLocationProvider,
            base::Unretained(GetContentClient()->browser())),
        std::move(java_nfc_delegate), std::move(request));
#else
    auto service = device::CreateDeviceService(
        device_blocking_task_runner, service_manager_thread_task_runner_,
        base::MakeRefCounted<DeviceServiceURLLoaderFactory>(),
        GetContentClient()->browser()->GetGeolocationApiKey(),
        base::BindRepeating(
            &ContentBrowserClient::OverrideSystemLocationProvider,
            base::Unretained(GetContentClient()->browser())),
        std::move(request));
#endif
    service_manager::Service::RunAsyncUntilTermination(std::move(service));
    return;
  }

  GetContentClient()->browser()->HandleServiceRequest(service_name,
                                                      std::move(request));
}

}  // namespace content
