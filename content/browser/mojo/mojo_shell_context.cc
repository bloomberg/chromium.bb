// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_shell_context.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/mojo/constants.h"
#include "content/common/mojo/mojo_shell_connection_impl.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/mojo_shell_connection.h"
#include "mojo/edk/embedder/embedder.h"
#include "services/catalog/catalog.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/store.h"
#include "services/file/public/cpp/constants.h"
#include "services/shell/connect_params.h"
#include "services/shell/native_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/interfaces/service.mojom.h"
#include "services/shell/runner/common/client_util.h"
#include "services/shell/runner/host/in_process_native_runner.h"
#include "services/shell/service_manager.h"

namespace content {

namespace {

base::LazyInstance<std::unique_ptr<shell::Connector>>::Leaky
    g_io_thread_connector = LAZY_INSTANCE_INITIALIZER;

void DestroyConnectorOnIOThread() { g_io_thread_connector.Get().reset(); }

void StartUtilityProcessOnIOThread(shell::mojom::ServiceFactoryRequest request,
                                   const base::string16& process_name,
                                   bool use_sandbox) {
  UtilityProcessHost* process_host =
      UtilityProcessHost::Create(nullptr, nullptr);
  process_host->SetName(process_name);
  if (!use_sandbox)
    process_host->DisableSandbox();
  process_host->Start();
  process_host->GetRemoteInterfaces()->GetInterface(std::move(request));
}

void StartServiceInUtilityProcess(const std::string& service_name,
                                  const base::string16& process_name,
                                  bool use_sandbox,
                                  shell::mojom::ServiceRequest request) {
  shell::mojom::ServiceFactoryPtr service_factory;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&StartUtilityProcessOnIOThread,
                                     base::Passed(GetProxy(&service_factory)),
                                     process_name, use_sandbox));
  service_factory->CreateService(std::move(request), service_name);
}

#if (ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)

// Request shell::mojom::ServiceFactory from GPU process host. Must be called on
// IO thread.
void RequestGpuServiceFactory(shell::mojom::ServiceFactoryRequest request) {
  GpuProcessHost* process_host =
      GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED);
  if (!process_host) {
    DLOG(ERROR) << "GPU process host not available.";
    return;
  }

  // TODO(xhwang): It's possible that |process_host| is non-null, but the actual
  // process is dead. In that case, |request| will be dropped and application
  // load requests through ServiceFactory will also fail. Make sure we handle
  // these cases correctly.
  process_host->GetRemoteInterfaces()->GetInterface(std::move(request));
}

void StartServiceInGpuProcess(const std::string& service_name,
                              shell::mojom::ServiceRequest request) {
  shell::mojom::ServiceFactoryPtr service_factory;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RequestGpuServiceFactory,
                 base::Passed(GetProxy(&service_factory))));
  service_factory->CreateService(std::move(request), service_name);
}

#endif  // ENABLE_MOJO_MEDIA_IN_GPU_PROCESS

// A ManifestProvider which resolves application names to builtin manifest
// resources for the catalog service to consume.
class BuiltinManifestProvider : public catalog::ManifestProvider {
 public:
  BuiltinManifestProvider() {}
  ~BuiltinManifestProvider() override {}

  void AddManifests(std::unique_ptr<
      ContentBrowserClient::MojoApplicationManifestMap> manifests) {
    DCHECK(!manifests_);
    manifests_ = std::move(manifests);
  }

  void AddManifestResource(const std::string& name, int resource_id) {
    std::string contents = GetContentClient()->GetDataResource(
        resource_id, ui::ScaleFactor::SCALE_FACTOR_NONE).as_string();
    DCHECK(!contents.empty());

    DCHECK(manifests_);
    auto result = manifests_->insert(std::make_pair(name, contents));
    DCHECK(result.second) << "Duplicate manifest entry: " << name;
  }

 private:
  // catalog::ManifestProvider:
  bool GetApplicationManifest(const base::StringPiece& name,
                              std::string* manifest_contents) override {
    auto manifest_it = manifests_->find(name.as_string());
    if (manifest_it != manifests_->end()) {
      *manifest_contents = manifest_it->second;
      DCHECK(!manifest_contents->empty());
      return true;
    }
    return false;
  }

  std::unique_ptr<ContentBrowserClient::MojoApplicationManifestMap> manifests_;

  DISALLOW_COPY_AND_ASSIGN(BuiltinManifestProvider);
};

}  // namespace

// State which lives on the IO thread and drives the ServiceManager.
class MojoShellContext::InProcessServiceManagerContext
    : public base::RefCountedThreadSafe<InProcessServiceManagerContext> {
 public:
  InProcessServiceManagerContext() {}

  shell::mojom::ServiceRequest Start(
      std::unique_ptr<BuiltinManifestProvider> manifest_provider) {
    shell::mojom::ServicePtr embedder_service_proxy;
    shell::mojom::ServiceRequest embedder_service_request =
        mojo::GetProxy(&embedder_service_proxy);
    shell::mojom::ServicePtrInfo embedder_service_proxy_info =
        embedder_service_proxy.PassInterface();
    BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)->PostTask(
        FROM_HERE,
        base::Bind(&InProcessServiceManagerContext::StartOnIOThread, this,
                   base::Passed(&manifest_provider),
                   base::Passed(&embedder_service_proxy_info)));
    return embedder_service_request;
  }

  void ShutDown() {
    BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)->PostTask(
        FROM_HERE,
        base::Bind(&InProcessServiceManagerContext::ShutDownOnIOThread, this));
  }

 private:
  friend class base::RefCountedThreadSafe<InProcessServiceManagerContext>;

  ~InProcessServiceManagerContext() {}

  void StartOnIOThread(
      std::unique_ptr<BuiltinManifestProvider> manifest_provider,
      shell::mojom::ServicePtrInfo embedder_service_proxy_info) {
    manifest_provider_ = std::move(manifest_provider);

    base::SequencedWorkerPool* blocking_pool = BrowserThread::GetBlockingPool();
    std::unique_ptr<shell::NativeRunnerFactory> native_runner_factory(
        new shell::InProcessNativeRunnerFactory(blocking_pool));
    catalog_.reset(
        new catalog::Catalog(blocking_pool, nullptr, manifest_provider_.get()));
    service_manager_.reset(new shell::ServiceManager(
        std::move(native_runner_factory), catalog_->TakeService()));

    shell::mojom::ServiceRequest request =
        service_manager_->StartEmbedderService(kBrowserMojoApplicationName);
    mojo::FuseInterface(
        std::move(request), std::move(embedder_service_proxy_info));
  }

  void ShutDownOnIOThread() {
    service_manager_.reset();
    catalog_.reset();
    manifest_provider_.reset();
  }

  std::unique_ptr<BuiltinManifestProvider> manifest_provider_;
  std::unique_ptr<catalog::Catalog> catalog_;
  std::unique_ptr<shell::ServiceManager> service_manager_;

  DISALLOW_COPY_AND_ASSIGN(InProcessServiceManagerContext);
};

MojoShellContext::MojoShellContext() {
  shell::mojom::ServiceRequest request;
  if (shell::ShellIsRemote()) {
    mojo::edk::SetParentPipeHandleFromCommandLine();
    request = shell::GetServiceRequestFromCommandLine();
  } else {
    // Allow the embedder to register additional Mojo application manifests
    // beyond the default ones below.
    std::unique_ptr<ContentBrowserClient::MojoApplicationManifestMap> manifests(
        new ContentBrowserClient::MojoApplicationManifestMap);
    GetContentClient()->browser()->RegisterMojoApplicationManifests(
        manifests.get());
    std::unique_ptr<BuiltinManifestProvider> manifest_provider =
        base::MakeUnique<BuiltinManifestProvider>();
    manifest_provider->AddManifests(std::move(manifests));
    manifest_provider->AddManifestResource(kBrowserMojoApplicationName,
                                           IDR_MOJO_CONTENT_BROWSER_MANIFEST);
    manifest_provider->AddManifestResource(kGpuMojoApplicationName,
                                           IDR_MOJO_CONTENT_GPU_MANIFEST);
    manifest_provider->AddManifestResource(kPluginMojoApplicationName,
                                           IDR_MOJO_CONTENT_PLUGIN_MANIFEST);
    manifest_provider->AddManifestResource(kRendererMojoApplicationName,
                                           IDR_MOJO_CONTENT_RENDERER_MANIFEST);
    manifest_provider->AddManifestResource(kUtilityMojoApplicationName,
                                           IDR_MOJO_CONTENT_UTILITY_MANIFEST);
    manifest_provider->AddManifestResource("mojo:catalog",
                                           IDR_MOJO_CATALOG_MANIFEST);
    manifest_provider->AddManifestResource(file::kFileServiceName,
                                           IDR_MOJO_FILE_MANIFEST);

    in_process_context_ = new InProcessServiceManagerContext;
    request = in_process_context_->Start(std::move(manifest_provider));
  }
  MojoShellConnection::SetForProcess(MojoShellConnection::Create(
      std::move(request),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)));

  ContentBrowserClient::StaticMojoApplicationMap apps;
  GetContentClient()->browser()->RegisterInProcessMojoApplications(&apps);
  for (const auto& entry : apps) {
    MojoShellConnection::GetForProcess()->AddEmbeddedService(entry.first,
                                                             entry.second);
  }

  // This is safe to assign directly from any thread, because MojoShellContext
  // must be constructed before anyone can call GetConnectorForIOThread().
  g_io_thread_connector.Get() =
      MojoShellConnection::GetForProcess()->GetConnector()->Clone();

  MojoShellConnection::GetForProcess()->Start();

  ContentBrowserClient::OutOfProcessMojoApplicationMap sandboxed_apps;
  GetContentClient()
      ->browser()
      ->RegisterOutOfProcessMojoApplications(&sandboxed_apps);
  for (const auto& app : sandboxed_apps) {
    MojoShellConnection::GetForProcess()->AddServiceRequestHandler(
        app.first,
        base::Bind(&StartServiceInUtilityProcess, app.first, app.second,
                   true /* use_sandbox */));
  }

  ContentBrowserClient::OutOfProcessMojoApplicationMap unsandboxed_apps;
  GetContentClient()
      ->browser()
      ->RegisterUnsandboxedOutOfProcessMojoApplications(&unsandboxed_apps);
  for (const auto& app : unsandboxed_apps) {
    MojoShellConnection::GetForProcess()->AddServiceRequestHandler(
        app.first,
        base::Bind(&StartServiceInUtilityProcess, app.first, app.second,
                   false /* use_sandbox */));
  }

#if (ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  MojoShellConnection::GetForProcess()->AddServiceRequestHandler(
      "mojo:media", base::Bind(&StartServiceInGpuProcess, "mojo:media"));
#endif
}

MojoShellContext::~MojoShellContext() {
  if (MojoShellConnection::GetForProcess())
    MojoShellConnection::DestroyForProcess();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&DestroyConnectorOnIOThread));
  if (in_process_context_)
    in_process_context_->ShutDown();
}

// static
shell::Connector* MojoShellContext::GetConnectorForIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return g_io_thread_connector.Get().get();
}

}  // namespace content
