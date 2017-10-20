// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_manager/service_manager_context.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/service_manager/common_browser_interfaces.h"
#include "content/browser/utility_process_host_impl.h"
#include "content/browser/wake_lock/wake_lock_context_host.h"
#include "content/common/service_manager/service_manager_connection_impl.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_service_registry.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "device/geolocation/geolocation_provider.h"
#include "media/mojo/features.h"
#include "media/mojo/interfaces/constants.mojom.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/incoming_broker_client_invitation.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/public/cpp/manifest_parsing_util.h"
#include "services/catalog/public/interfaces/constants.mojom.h"
#include "services/data_decoder/public/interfaces/constants.mojom.h"
#include "services/device/device_service.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/resource_coordinator/resource_coordinator_service.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/embedder/manifest_utils.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/host/service_process_launcher.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/service_manager.h"
#include "services/shape_detection/public/interfaces/constants.mojom.h"
#include "services/video_capture/public/cpp/constants.h"
#include "services/video_capture/public/interfaces/constants.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "jni/ContentNfcDelegate_jni.h"
#endif

#if defined(OS_WIN)
#include "content/browser/renderer_host/dwrite_font_proxy_message_filter_win.h"
#endif

namespace content {

namespace {

base::LazyInstance<std::unique_ptr<service_manager::Connector>>::Leaky
    g_io_thread_connector = LAZY_INSTANCE_INITIALIZER;

void DestroyConnectorOnIOThread() { g_io_thread_connector.Get().reset(); }

// Launch a process for a service once its sandbox type is known.
void StartServiceInUtilityProcess(
    const std::string& service_name,
    const base::string16& process_name,
    service_manager::mojom::ServiceRequest request,
    service_manager::mojom::ConnectResult query_result,
    const std::string& sandbox_string) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_manager::SandboxType sandbox_type =
      service_manager::UtilitySandboxTypeFromString(sandbox_string);

  UtilityProcessHostImpl* process_host =
      new UtilityProcessHostImpl(nullptr, nullptr);
#if defined(OS_WIN)
  if (sandbox_type == service_manager::SANDBOX_TYPE_PPAPI)
    process_host->AddFilter(new DWriteFontProxyMessageFilter());
#endif
  process_host->SetName(process_name);
  process_host->SetServiceIdentity(service_manager::Identity(service_name));
  process_host->SetSandboxType(sandbox_type);
  process_host->Start();

  service_manager::mojom::ServiceFactoryPtr service_factory;
  BindInterface(process_host, mojo::MakeRequest(&service_factory));
  service_factory->CreateService(std::move(request), service_name);
}

// Determine a sandbox type for a service and launch a process for it.
void QueryAndStartServiceInUtilityProcess(
    const std::string& service_name,
    const base::string16& process_name,
    service_manager::mojom::ServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceManagerContext::GetConnectorForIOThread()->QueryService(
      service_manager::Identity(service_name),
      base::BindOnce(&StartServiceInUtilityProcess, service_name, process_name,
                     std::move(request)));
}

// Request service_manager::mojom::ServiceFactory from GPU process host. Must be
// called on IO thread.
void StartServiceInGpuProcess(const std::string& service_name,
                              service_manager::mojom::ServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GpuProcessHost* process_host = GpuProcessHost::Get();
  if (!process_host) {
    DLOG(ERROR) << "GPU process host not available.";
    return;
  }

  service_manager::mojom::ServiceFactoryPtr service_factory;
  // TODO(xhwang): It's possible that |process_host| is non-null, but the actual
  // process is dead. In that case, |request| will be dropped and application
  // load requests through ServiceFactory will also fail. Make sure we handle
  // these cases correctly.
  BindInterfaceInGpuProcess(mojo::MakeRequest(&service_factory));
  service_factory->CreateService(std::move(request), service_name);
}

// A ManifestProvider which resolves application names to builtin manifest
// resources for the catalog service to consume.
class BuiltinManifestProvider : public catalog::ManifestProvider {
 public:
  BuiltinManifestProvider() {}
  ~BuiltinManifestProvider() override {}

  void AddServiceManifest(base::StringPiece name, int resource_id) {
    std::string contents =
        GetContentClient()
            ->GetDataResource(resource_id, ui::ScaleFactor::SCALE_FACTOR_NONE)
            .as_string();
    DCHECK(!contents.empty());

    std::unique_ptr<base::Value> manifest_value =
        base::JSONReader::Read(contents);
    DCHECK(manifest_value);

    std::unique_ptr<base::Value> overlay_value =
        GetContentClient()->browser()->GetServiceManifestOverlay(name);

    service_manager::MergeManifestWithOverlay(manifest_value.get(),
                                              overlay_value.get());

    base::Optional<catalog::RequiredFileMap> required_files =
        catalog::RetrieveRequiredFiles(*manifest_value);
    if (required_files) {
      ChildProcessLauncher::SetRegisteredFilesForService(
          name.as_string(), std::move(*required_files));
    }

    auto result = manifests_.insert(
        std::make_pair(name.as_string(), std::move(manifest_value)));
    DCHECK(result.second) << "Duplicate manifest entry: " << name;
  }

 private:
  // catalog::ManifestProvider:
  std::unique_ptr<base::Value> GetManifest(const std::string& name) override {
    auto it = manifests_.find(name);
    return it != manifests_.end() ? it->second->CreateDeepCopy() : nullptr;
  }

  std::map<std::string, std::unique_ptr<base::Value>> manifests_;

  DISALLOW_COPY_AND_ASSIGN(BuiltinManifestProvider);
};

class NullServiceProcessLauncherFactory
    : public service_manager::ServiceProcessLauncherFactory {
 public:
  NullServiceProcessLauncherFactory() {}
  ~NullServiceProcessLauncherFactory() override {}

 private:
  std::unique_ptr<service_manager::ServiceProcessLauncher> Create(
      const base::FilePath& service_path) override {
    LOG(ERROR) << "Attempting to run unsupported native service: "
               << service_path.value();
    return nullptr;
  }

  DISALLOW_COPY_AND_ASSIGN(NullServiceProcessLauncherFactory);
};

// Helper to invoke GetGeolocationRequestContext on the currently-set
// ContentBrowserClient.
void GetGeolocationRequestContextFromContentClient(
    base::OnceCallback<void(scoped_refptr<net::URLRequestContextGetter>)>
        callback) {
  GetContentClient()->browser()->GetGeolocationRequestContext(
      std::move(callback));
}

}  // namespace

// State which lives on the IO thread and drives the ServiceManager.
class ServiceManagerContext::InProcessServiceManagerContext
    : public base::RefCountedThreadSafe<InProcessServiceManagerContext> {
 public:
  InProcessServiceManagerContext() {}

  void Start(
      service_manager::mojom::ServicePtrInfo packaged_services_service_info,
      std::unique_ptr<BuiltinManifestProvider> manifest_provider) {
    BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&InProcessServiceManagerContext::StartOnIOThread,
                           this, base::Passed(&manifest_provider),
                           base::Passed(&packaged_services_service_info)));
  }

  void ShutDown() {
    BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&InProcessServiceManagerContext::ShutDownOnIOThread,
                           this));
  }

 private:
  friend class base::RefCountedThreadSafe<InProcessServiceManagerContext>;

  ~InProcessServiceManagerContext() {}

  void StartOnIOThread(
      std::unique_ptr<BuiltinManifestProvider> manifest_provider,
      service_manager::mojom::ServicePtrInfo packaged_services_service_info) {
    manifest_provider_ = std::move(manifest_provider);
    service_manager_ = base::MakeUnique<service_manager::ServiceManager>(
        base::MakeUnique<NullServiceProcessLauncherFactory>(), nullptr,
        manifest_provider_.get());

    service_manager::mojom::ServicePtr packaged_services_service;
    packaged_services_service.Bind(std::move(packaged_services_service_info));
    service_manager_->RegisterService(
        service_manager::Identity(mojom::kPackagedServicesServiceName,
                                  service_manager::mojom::kRootUserID),
        std::move(packaged_services_service), nullptr);
    service_manager_->SetInstanceQuitCallback(
        base::Bind(&OnInstanceQuitOnIOThread));
  }

  static void OnInstanceQuitOnIOThread(const service_manager::Identity& id) {
    BrowserThread::GetTaskRunnerForThread(BrowserThread::UI)
        ->PostTask(FROM_HERE, base::BindOnce(&OnInstanceQuit, id));
  }

  static void OnInstanceQuit(const service_manager::Identity& id) {
    if (GetContentClient()->browser()->ShouldTerminateOnServiceQuit(id)) {
      LOG(FATAL) << "Terminating because service '" << id.name()
                 << "' quit unexpectedly.";
    }
  }

  void ShutDownOnIOThread() {
    service_manager_.reset();
    manifest_provider_.reset();
  }

  std::unique_ptr<BuiltinManifestProvider> manifest_provider_;
  std::unique_ptr<service_manager::ServiceManager> service_manager_;

  DISALLOW_COPY_AND_ASSIGN(InProcessServiceManagerContext);
};

ServiceManagerContext::ServiceManagerContext() {
  service_manager::mojom::ServiceRequest packaged_services_request;
  if (service_manager::ServiceManagerIsRemote()) {
    auto invitation =
        mojo::edk::IncomingBrokerClientInvitation::AcceptFromCommandLine(
            mojo::edk::TransportProtocol::kLegacy);
    packaged_services_request =
        service_manager::GetServiceRequestFromCommandLine(invitation.get());
  } else {
    std::unique_ptr<BuiltinManifestProvider> manifest_provider =
        base::MakeUnique<BuiltinManifestProvider>();

    static const struct ManifestInfo {
      const char* name;
      int resource_id;
    } kManifests[] = {
        {mojom::kBrowserServiceName, IDR_MOJO_CONTENT_BROWSER_MANIFEST},
        {mojom::kGpuServiceName, IDR_MOJO_CONTENT_GPU_MANIFEST},
        {mojom::kPackagedServicesServiceName,
         IDR_MOJO_CONTENT_PACKAGED_SERVICES_MANIFEST},
        {mojom::kPluginServiceName, IDR_MOJO_CONTENT_PLUGIN_MANIFEST},
        {mojom::kRendererServiceName, IDR_MOJO_CONTENT_RENDERER_MANIFEST},
        {mojom::kUtilityServiceName, IDR_MOJO_CONTENT_UTILITY_MANIFEST},
        {catalog::mojom::kServiceName, IDR_MOJO_CATALOG_MANIFEST},
    };

    for (size_t i = 0; i < arraysize(kManifests); ++i) {
      manifest_provider->AddServiceManifest(kManifests[i].name,
                                            kManifests[i].resource_id);
    }
    for (const auto& manifest :
         GetContentClient()->browser()->GetExtraServiceManifests()) {
      manifest_provider->AddServiceManifest(manifest.name,
                                            manifest.resource_id);
    }
    in_process_context_ = new InProcessServiceManagerContext;

    service_manager::mojom::ServicePtr packaged_services_service;
    packaged_services_request = mojo::MakeRequest(&packaged_services_service);
    in_process_context_->Start(packaged_services_service.PassInterface(),
                               std::move(manifest_provider));
  }

  packaged_services_connection_ = ServiceManagerConnection::Create(
      std::move(packaged_services_request),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));

  service_manager::mojom::ServicePtr root_browser_service;
  ServiceManagerConnection::SetForProcess(ServiceManagerConnection::Create(
      mojo::MakeRequest(&root_browser_service),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)));
  auto* browser_connection = ServiceManagerConnection::GetForProcess();

  service_manager::mojom::PIDReceiverPtr pid_receiver;
  packaged_services_connection_->GetConnector()->StartService(
      service_manager::Identity(mojom::kBrowserServiceName,
                                service_manager::mojom::kRootUserID),
      std::move(root_browser_service), mojo::MakeRequest(&pid_receiver));
  pid_receiver->SetPID(base::GetCurrentProcId());

  service_manager::EmbeddedServiceInfo device_info;

  // This task runner may be used by some device service implementation bits to
  // interface with dbus client code, which in turn imposes some subtle thread
  // affinity on the clients. We therefore require a single-thread runner.
  scoped_refptr<base::SingleThreadTaskRunner> device_blocking_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND});

#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> java_nfc_delegate;
  java_nfc_delegate.Reset(Java_ContentNfcDelegate_create(env));
  DCHECK(!java_nfc_delegate.is_null());

  // See the comments on wake_lock_context_host.h and ContentNfcDelegate.java
  // respectively for comments on those parameters.
  device_info.factory =
      base::Bind(&device::CreateDeviceService, device_blocking_task_runner,
                 BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
                 base::Bind(&WakeLockContextHost::GetNativeViewForContext),
                 std::move(java_nfc_delegate));
#else
  device_info.factory =
      base::Bind(&device::CreateDeviceService, device_blocking_task_runner,
                 BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
#endif
  device_info.task_runner = base::ThreadTaskRunnerHandle::Get();
  packaged_services_connection_->AddEmbeddedService(device::mojom::kServiceName,
                                                    device_info);

  // Pipe embedder-supplied API key through to GeolocationProvider.
  // TODO(amoylan): Once GeolocationProvider hangs off DeviceService
  // (https://crbug.com/709301), pass these via CreateDeviceService above
  // instead.
  device::GeolocationProvider::SetRequestContextProducer(
      base::BindRepeating(&GetGeolocationRequestContextFromContentClient));
  device::GeolocationProvider::SetApiKey(
      GetContentClient()->browser()->GetGeolocationApiKey());

  if (base::FeatureList::IsEnabled(features::kGlobalResourceCoordinator)) {
    service_manager::EmbeddedServiceInfo resource_coordinator_info;
    resource_coordinator_info.factory =
        base::Bind(&resource_coordinator::ResourceCoordinatorService::Create);
    packaged_services_connection_->AddEmbeddedService(
        resource_coordinator::mojom::kServiceName, resource_coordinator_info);
  }

  ContentBrowserClient::StaticServiceMap services;
  GetContentClient()->browser()->RegisterInProcessServices(&services);
  for (const auto& entry : services) {
    packaged_services_connection_->AddEmbeddedService(entry.first,
                                                      entry.second);
  }

  // This is safe to assign directly from any thread, because
  // ServiceManagerContext must be constructed before anyone can call
  // GetConnectorForIOThread().
  g_io_thread_connector.Get() = browser_connection->GetConnector()->Clone();

  ContentBrowserClient::OutOfProcessServiceMap out_of_process_services;
  GetContentClient()->browser()->RegisterOutOfProcessServices(
      &out_of_process_services);

  out_of_process_services[data_decoder::mojom::kServiceName] =
      base::ASCIIToUTF16("Data Decoder Service");

  bool network_service_enabled =
      base::FeatureList::IsEnabled(features::kNetworkService);
  if (network_service_enabled) {
    out_of_process_services[content::mojom::kNetworkServiceName] =
        base::ASCIIToUTF16("Network Service");
  }

  if (base::FeatureList::IsEnabled(video_capture::kMojoVideoCapture)) {
    out_of_process_services[video_capture::mojom::kServiceName] =
        base::ASCIIToUTF16("Video Capture Service");
  }

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
  out_of_process_services[media::mojom::kMediaServiceName] =
      base::ASCIIToUTF16("Media Service");
#endif

#if BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE)
  out_of_process_services[media::mojom::kCdmServiceName] =
      base::ASCIIToUTF16("Content Decryption Module Service");
#endif

  for (const auto& service : out_of_process_services) {
    packaged_services_connection_->AddServiceRequestHandler(
        service.first, base::Bind(&QueryAndStartServiceInUtilityProcess,
                                  service.first, service.second));
  }

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  packaged_services_connection_->AddServiceRequestHandler(
      media::mojom::kMediaServiceName,
      base::Bind(&StartServiceInGpuProcess, media::mojom::kMediaServiceName));
#endif

  packaged_services_connection_->AddServiceRequestHandler(
      shape_detection::mojom::kServiceName,
      base::Bind(&StartServiceInGpuProcess,
                 shape_detection::mojom::kServiceName));

  packaged_services_connection_->Start();

  RegisterCommonBrowserInterfaces(browser_connection);
  browser_connection->Start();

  if (network_service_enabled) {
    // Start the network service process as soon as possible, since it is
    // critical to start up performance.
    browser_connection->GetConnector()->StartService(
        mojom::kNetworkServiceName);
  }
}

ServiceManagerContext::~ServiceManagerContext() {
  // NOTE: The in-process ServiceManager MUST be destroyed before the browser
  // process-wide ServiceManagerConnection. Otherwise it's possible for the
  // ServiceManager to receive connection requests for service:content_browser
  // which it may attempt to service by launching a new instance of the browser.
  if (in_process_context_)
    in_process_context_->ShutDown();
  if (ServiceManagerConnection::GetForProcess())
    ServiceManagerConnection::DestroyForProcess();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(&DestroyConnectorOnIOThread));
}

// static
service_manager::Connector* ServiceManagerContext::GetConnectorForIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return g_io_thread_connector.Get().get();
}

}  // namespace content
