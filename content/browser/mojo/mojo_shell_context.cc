// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_shell_context.h"

#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/mojo/constants.h"
#include "content/common/mojo/mojo_shell_connection_impl.h"
#include "content/common/process_control.mojom.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/string.h"
#include "services/catalog/catalog.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/store.h"
#include "services/shell/connect_params.h"
#include "services/shell/native_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/service.mojom.h"
#include "services/shell/public/interfaces/service_factory.mojom.h"
#include "services/shell/runner/common/client_util.h"
#include "services/shell/runner/host/in_process_native_runner.h"
#include "services/user/public/cpp/constants.h"

namespace content {

namespace {

base::LazyInstance<std::unique_ptr<shell::Connector>>::Leaky
    g_io_thread_connector = LAZY_INSTANCE_INITIALIZER;

void DestroyConnectorOnIOThread() { g_io_thread_connector.Get().reset(); }

void StartUtilityProcessOnIOThread(
    mojo::InterfaceRequest<mojom::ProcessControl> request,
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

void OnApplicationLoaded(const std::string& name, bool success) {
  if (!success)
    LOG(ERROR) << "Failed to launch Mojo application for " << name;
}

void LaunchAppInUtilityProcess(const std::string& app_name,
                               const base::string16& process_name,
                               bool use_sandbox,
                               shell::mojom::ServiceRequest request) {
  mojom::ProcessControlPtr process_control;
  mojom::ProcessControlRequest process_request =
      mojo::GetProxy(&process_control);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&StartUtilityProcessOnIOThread,
                                     base::Passed(&process_request),
                                     process_name, use_sandbox));
  process_control->LoadApplication(app_name, std::move(request),
                                   base::Bind(&OnApplicationLoaded, app_name));
}

#if (ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)

// Request mojom::ProcessControl from GPU process host. Must be called on IO
// thread.
void RequestGpuProcessControl(
    mojo::InterfaceRequest<mojom::ProcessControl> request) {
  BrowserChildProcessHostDelegate* process_host =
      GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED);
  if (!process_host) {
    DLOG(ERROR) << "GPU process host not available.";
    return;
  }

  // TODO(xhwang): It's possible that |process_host| is non-null, but the actual
  // process is dead. In that case, |request| will be dropped and application
  // load requests through mojom::ProcessControl will also fail. Make sure we
  // handle
  // these cases correctly.
  process_host->GetRemoteInterfaces()->GetInterface(std::move(request));
}

void LaunchAppInGpuProcess(const std::string& app_name,
                           shell::mojom::ServiceRequest request) {
  mojom::ProcessControlPtr process_control;
  mojom::ProcessControlRequest process_request =
      mojo::GetProxy(&process_control);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RequestGpuProcessControl, base::Passed(&process_request)));
  process_control->LoadApplication(app_name, std::move(request),
                                   base::Bind(&OnApplicationLoaded, app_name));
}

#endif  // ENABLE_MOJO_MEDIA_IN_GPU_PROCESS

}  // namespace

// A ManifestProvider which resolves application names to builtin manifest
// resources for the catalog service to consume.
class MojoShellContext::BuiltinManifestProvider
    : public catalog::ManifestProvider {
 public:
  BuiltinManifestProvider() {}
  ~BuiltinManifestProvider() override {}

  void AddManifestResource(const std::string& name, int resource_id) {
    auto result = manifest_resources_.insert(
        std::make_pair(name, resource_id));
    DCHECK(result.second);
  }

  void AddManifests(std::unique_ptr<
      ContentBrowserClient::MojoApplicationManifestMap> manifests) {
    manifests_ = std::move(manifests);
  }

 private:
  // catalog::ManifestProvider:
  bool GetApplicationManifest(const base::StringPiece& name,
                              std::string* manifest_contents) override {
    auto it = manifest_resources_.find(name.as_string());
    if (it != manifest_resources_.end()) {
      *manifest_contents =
          GetContentClient()
              ->GetDataResource(it->second, ui::ScaleFactor::SCALE_FACTOR_NONE)
              .as_string();
      DCHECK(!manifest_contents->empty());
      return true;
    }
    auto manifest_it = manifests_->find(name.as_string());
    if (manifest_it != manifests_->end()) {
      *manifest_contents = manifest_it->second;
      DCHECK(!manifest_contents->empty());
      return true;
    }
    return false;
  }

  std::unordered_map<std::string, int> manifest_resources_;
  std::unique_ptr<ContentBrowserClient::MojoApplicationManifestMap> manifests_;

  DISALLOW_COPY_AND_ASSIGN(BuiltinManifestProvider);
};

MojoShellContext::MojoShellContext() {
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE);
  std::unique_ptr<shell::NativeRunnerFactory> native_runner_factory(
      new shell::InProcessNativeRunnerFactory(
          BrowserThread::GetBlockingPool()));

  // Allow the embedder to register additional Mojo application manifests
  // beyond the default ones below.
  std::unique_ptr<ContentBrowserClient::MojoApplicationManifestMap> manifests(
      new ContentBrowserClient::MojoApplicationManifestMap);
  GetContentClient()->browser()->RegisterMojoApplicationManifests(
      manifests.get());

  manifest_provider_.reset(new BuiltinManifestProvider);
  manifest_provider_->AddManifests(std::move(manifests));
  manifest_provider_->AddManifestResource(kBrowserMojoApplicationName,
                                          IDR_MOJO_CONTENT_BROWSER_MANIFEST);
  manifest_provider_->AddManifestResource(kGpuMojoApplicationName,
                                          IDR_MOJO_CONTENT_GPU_MANIFEST);
  manifest_provider_->AddManifestResource(kRendererMojoApplicationName,
                                          IDR_MOJO_CONTENT_RENDERER_MANIFEST);
  manifest_provider_->AddManifestResource(kUtilityMojoApplicationName,
                                          IDR_MOJO_CONTENT_UTILITY_MANIFEST);
  manifest_provider_->AddManifestResource("mojo:catalog",
                                          IDR_MOJO_CATALOG_MANIFEST);
  manifest_provider_->AddManifestResource(user_service::kUserServiceName,
                                          IDR_MOJO_PROFILE_MANIFEST);

  catalog_.reset(new catalog::Catalog(file_task_runner.get(), nullptr,
                                      manifest_provider_.get()));

  shell::mojom::ServiceRequest request;
  if (shell::ShellIsRemote()) {
    mojo::edk::SetParentPipeHandleFromCommandLine();
    request = shell::GetServiceRequestFromCommandLine();
  } else {
    service_manager_.reset(new shell::ServiceManager(
        std::move(native_runner_factory), catalog_->TakeService()));
    request =
        service_manager_->StartEmbedderService(kBrowserMojoApplicationName);
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
        base::Bind(&LaunchAppInUtilityProcess, app.first, app.second,
                   true /* use_sandbox */));
  }

  ContentBrowserClient::OutOfProcessMojoApplicationMap unsandboxed_apps;
  GetContentClient()
      ->browser()
      ->RegisterUnsandboxedOutOfProcessMojoApplications(&unsandboxed_apps);
  for (const auto& app : unsandboxed_apps) {
    MojoShellConnection::GetForProcess()->AddServiceRequestHandler(
        app.first,
        base::Bind(&LaunchAppInUtilityProcess, app.first, app.second,
                   false /* use_sandbox */));
  }

#if (ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  MojoShellConnection::GetForProcess()->AddServiceRequestHandler(
      "mojo:media", base::Bind(&LaunchAppInGpuProcess, "mojo:media"));
#endif
}

MojoShellContext::~MojoShellContext() {
  if (MojoShellConnection::GetForProcess())
    MojoShellConnection::DestroyForProcess();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&DestroyConnectorOnIOThread));
  catalog_.reset();
}

// static
shell::Connector* MojoShellContext::GetConnectorForIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return g_io_thread_connector.Get().get();
}

}  // namespace content
