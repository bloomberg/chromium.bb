// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_shell_context.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/profile_service/profile_app.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/mojo/mojo_shell_connection_impl.h"
#include "content/common/mojo/static_loader.h"
#include "content/common/process_control.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_registry.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/shell/connect_params.h"
#include "mojo/shell/loader.h"
#include "mojo/shell/native_runner.h"
#include "mojo/shell/public/cpp/identity.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"
#include "mojo/shell/runner/host/in_process_native_runner.h"

namespace content {

namespace {

const char kBrowserAppName[] = "exe:chrome";

// An extra set of apps to register on initialization, if set by a test.
const MojoShellContext::StaticApplicationMap* g_applications_for_test;

void StartUtilityProcessOnIOThread(
    mojo::InterfaceRequest<ProcessControl> request,
    const base::string16& process_name,
    bool use_sandbox) {
  UtilityProcessHost* process_host =
      UtilityProcessHost::Create(nullptr, nullptr);
  process_host->SetName(process_name);
  if (!use_sandbox)
    process_host->DisableSandbox();
  process_host->StartMojoMode();

  ServiceRegistry* services = process_host->GetServiceRegistry();
  services->ConnectToRemoteService(std::move(request));
}

void OnApplicationLoaded(const std::string& name, bool success) {
  if (!success)
    LOG(ERROR) << "Failed to launch Mojo application for " << name;
}

// The default loader to use for all applications. This does nothing but drop
// the Application request.
class DefaultLoader : public mojo::shell::Loader {
 public:
   DefaultLoader() {}
   ~DefaultLoader() override {}

 private:
  // mojo::shell::Loader:
  void Load(const std::string& name,
            mojo::shell::mojom::ShellClientRequest request) override {}

  DISALLOW_COPY_AND_ASSIGN(DefaultLoader);
};

// This launches a utility process and forwards the Load request the
// ProcessControl service there. The utility process is sandboxed iff
// |use_sandbox| is true.
class UtilityProcessLoader : public mojo::shell::Loader {
 public:
  UtilityProcessLoader(const base::string16& process_name, bool use_sandbox)
      : process_name_(process_name), use_sandbox_(use_sandbox) {}
  ~UtilityProcessLoader() override {}

 private:
  // mojo::shell::Loader:
  void Load(const std::string& name,
            mojo::shell::mojom::ShellClientRequest request) override {
    ProcessControlPtr process_control;
    auto process_request = mojo::GetProxy(&process_control);
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&StartUtilityProcessOnIOThread,
                                       base::Passed(&process_request),
                                       process_name_, use_sandbox_));
    process_control->LoadApplication(name, std::move(request),
                                     base::Bind(&OnApplicationLoaded, name));
  }

  const base::string16 process_name_;
  const bool use_sandbox_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessLoader);
};

// Request ProcessControl from GPU process host. Must be called on IO thread.
void RequestGpuProcessControl(mojo::InterfaceRequest<ProcessControl> request) {
  BrowserChildProcessHostDelegate* process_host =
      GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                          CAUSE_FOR_GPU_LAUNCH_MOJO_SETUP);
  if (!process_host) {
    DLOG(ERROR) << "GPU process host not available.";
    return;
  }

  // TODO(xhwang): It's possible that |process_host| is non-null, but the actual
  // process is dead. In that case, |request| will be dropped and application
  // load requests through ProcessControl will also fail. Make sure we handle
  // these cases correctly.
  process_host->GetServiceRegistry()->ConnectToRemoteService(
      std::move(request));
}

// Forwards the load request to the GPU process.
class GpuProcessLoader : public mojo::shell::Loader {
 public:
  GpuProcessLoader() {}
  ~GpuProcessLoader() override {}

 private:
  // mojo::shell::Loader:
  void Load(const std::string& name,
            mojo::shell::mojom::ShellClientRequest request) override {
    ProcessControlPtr process_control;
    auto process_request = mojo::GetProxy(&process_control);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&RequestGpuProcessControl, base::Passed(&process_request)));
    process_control->LoadApplication(name, std::move(request),
                                     base::Bind(&OnApplicationLoaded, name));
  }

  DISALLOW_COPY_AND_ASSIGN(GpuProcessLoader);
};

}  // namespace

// Thread-safe proxy providing access to the shell context from any thread.
class MojoShellContext::Proxy {
 public:
  Proxy(MojoShellContext* shell_context)
      : shell_context_(shell_context),
        task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

  ~Proxy() {}

  void ConnectToApplication(
      const std::string& name,
      const std::string& requestor_name,
      mojo::shell::mojom::InterfaceProviderRequest request,
      mojo::shell::mojom::InterfaceProviderPtr exposed_services,
      const mojo::shell::mojom::Connector::ConnectCallback& callback) {
    if (task_runner_ == base::ThreadTaskRunnerHandle::Get()) {
      if (shell_context_) {
        shell_context_->ConnectToApplicationOnOwnThread(
            name, requestor_name, std::move(request),
            std::move(exposed_services), callback);
      }
    } else {
      // |shell_context_| outlives the main MessageLoop, so it's safe for it to
      // be unretained here.
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&MojoShellContext::ConnectToApplicationOnOwnThread,
                     base::Unretained(shell_context_), name, requestor_name,
                     base::Passed(&request), base::Passed(&exposed_services),
                     callback));
    }
  }

 private:
  MojoShellContext* shell_context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Proxy);
};

// static
base::LazyInstance<scoped_ptr<MojoShellContext::Proxy>>
    MojoShellContext::proxy_ = LAZY_INSTANCE_INITIALIZER;

void MojoShellContext::SetApplicationsForTest(
    const StaticApplicationMap* apps) {
  g_applications_for_test = apps;
}

MojoShellContext::MojoShellContext() {
  proxy_.Get().reset(new Proxy(this));

  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
  scoped_ptr<mojo::shell::NativeRunnerFactory> native_runner_factory(
      new mojo::shell::InProcessNativeRunnerFactory(
          BrowserThread::GetBlockingPool()));
  shell_.reset(new mojo::shell::Shell(std::move(native_runner_factory),
                                      file_task_runner.get(), nullptr));

  shell_->set_default_loader(
      scoped_ptr<mojo::shell::Loader>(new DefaultLoader));

  StaticApplicationMap apps;
  GetContentClient()->browser()->RegisterInProcessMojoApplications(&apps);
  if (g_applications_for_test) {
    // Add testing apps to the map, potentially overwriting whatever the
    // browser client registered.
    for (const auto& entry : *g_applications_for_test)
      apps[entry.first] = entry.second;
  }
  for (const auto& entry : apps) {
    shell_->SetLoaderForName(
        make_scoped_ptr(new StaticLoader(entry.second)), entry.first);
  }

  ContentBrowserClient::OutOfProcessMojoApplicationMap sandboxed_apps;
  GetContentClient()
      ->browser()
      ->RegisterOutOfProcessMojoApplications(&sandboxed_apps);
  for (const auto& app : sandboxed_apps) {
    shell_->SetLoaderForName(
        make_scoped_ptr(
            new UtilityProcessLoader(app.second, true /* use_sandbox */)),
        app.first);
  }

  ContentBrowserClient::OutOfProcessMojoApplicationMap unsandboxed_apps;
  GetContentClient()
      ->browser()
      ->RegisterUnsandboxedOutOfProcessMojoApplications(&unsandboxed_apps);
  for (const auto& app : unsandboxed_apps) {
    shell_->SetLoaderForName(
        make_scoped_ptr(
            new UtilityProcessLoader(app.second, false /* use_sandbox */)),
        app.first);
  }

#if (ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  shell_->SetLoaderForName(make_scoped_ptr(new GpuProcessLoader), "mojo:media");
#endif

  base::Callback<scoped_ptr<mojo::ShellClient>()> profile_callback =
      base::Bind(&profile::CreateProfileApp);
  shell_->SetLoaderForName(
      make_scoped_ptr(new StaticLoader(profile_callback)), "mojo:profile");

  if (!IsRunningInMojoShell()) {
    MojoShellConnectionImpl::Create(
        shell_->InitInstanceForEmbedder(kBrowserAppName));
  }
}

MojoShellContext::~MojoShellContext() {
  if (!IsRunningInMojoShell())
    MojoShellConnectionImpl::Destroy();
}

// static
void MojoShellContext::ConnectToApplication(
    const std::string& name,
    const std::string& requestor_name,
    mojo::shell::mojom::InterfaceProviderRequest request,
    mojo::shell::mojom::InterfaceProviderPtr exposed_services,
    const mojo::shell::mojom::Connector::ConnectCallback& callback) {
  proxy_.Get()->ConnectToApplication(name, requestor_name, std::move(request),
                                     std::move(exposed_services), callback);
}

void MojoShellContext::ConnectToApplicationOnOwnThread(
    const std::string& name,
    const std::string& requestor_name,
    mojo::shell::mojom::InterfaceProviderRequest request,
    mojo::shell::mojom::InterfaceProviderPtr exposed_services,
    const mojo::shell::mojom::Connector::ConnectCallback& callback) {
  scoped_ptr<mojo::shell::ConnectParams> params(new mojo::shell::ConnectParams);
  // TODO(beng): kRootUserID is obviously wrong.
  mojo::Identity source_id(requestor_name, mojo::shell::mojom::kRootUserID);
  params->set_source(source_id);
  params->set_target(mojo::Identity(name, mojo::shell::mojom::kRootUserID));
  params->set_remote_interfaces(std::move(request));
  params->set_local_interfaces(std::move(exposed_services));
  params->set_connect_callback(callback);
  shell_->Connect(std::move(params));
}

}  // namespace content
