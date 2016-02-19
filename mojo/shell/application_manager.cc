// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/application_manager.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/services/package_manager/loader.h"
#include "mojo/shell/application_instance.h"
#include "mojo/shell/connect_util.h"
#include "mojo/shell/public/cpp/connect.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/shell/switches.h"
#include "mojo/util/filename_util.h"

namespace mojo {
namespace shell {

namespace {

void OnEmptyOnConnectCallback(uint32_t remote_id) {}

class ShellApplicationLoader : public ApplicationLoader {
 public:
  explicit ShellApplicationLoader(ApplicationManager* manager)
      : manager_(manager) {}
  ~ShellApplicationLoader() override {}

 private:
  // Overridden from ApplicationLoader:
  void Load(const GURL& url, mojom::ShellClientRequest request) override {
    DCHECK(request.is_pending());
    shell_connection_.reset(new ShellConnection(manager_, std::move(request)));
  }

  ApplicationManager* manager_;
  scoped_ptr<ShellConnection> shell_connection_;

  DISALLOW_COPY_AND_ASSIGN(ShellApplicationLoader);
};

}  // namespace

mojom::Shell::ConnectToApplicationCallback EmptyConnectCallback() {
  return base::Bind(&OnEmptyOnConnectCallback);
}

// static
ApplicationManager::TestAPI::TestAPI(ApplicationManager* manager)
    : manager_(manager) {
}

ApplicationManager::TestAPI::~TestAPI() {
}

bool ApplicationManager::TestAPI::HasRunningInstanceForURL(
    const GURL& url) const {
  return manager_->identity_to_instance_.find(Identity(url)) !=
         manager_->identity_to_instance_.end();
}

////////////////////////////////////////////////////////////////////////////////
// ApplicationManager, public:

ApplicationManager::ApplicationManager(
    bool register_mojo_url_schemes)
    : ApplicationManager(nullptr, nullptr, register_mojo_url_schemes) {}

ApplicationManager::ApplicationManager(
    scoped_ptr<NativeRunnerFactory> native_runner_factory,
    base::TaskRunner* task_runner,
    bool register_mojo_url_schemes)
    : task_runner_(task_runner),
      native_runner_factory_(std::move(native_runner_factory)),
      weak_ptr_factory_(this) {
  SetLoaderForURL(make_scoped_ptr(new ShellApplicationLoader(this)),
                  GURL("mojo://shell/"));

  GURL package_manager_url("mojo://package_manager/");
  SetLoaderForURL(make_scoped_ptr(new package_manager::Loader(
      task_runner_, register_mojo_url_schemes)), package_manager_url);

  ConnectToInterface(this, CreateShellIdentity(), package_manager_url,
                     &shell_resolver_);
}

ApplicationManager::~ApplicationManager() {
  TerminateShellConnections();
  STLDeleteValues(&url_to_loader_);
  for (auto& runner : native_runners_)
    runner.reset();
}

void ApplicationManager::ConnectToApplication(
    scoped_ptr<ConnectToApplicationParams> params) {
  TRACE_EVENT_INSTANT1("mojo_shell", "ApplicationManager::ConnectToApplication",
                       TRACE_EVENT_SCOPE_THREAD, "original_url",
                       params->target().url().spec());
  DCHECK(params->target().url().is_valid());

  // Connect to an existing matching instance, if possible.
  if (ConnectToRunningApplication(&params))
    return;

  // TODO(beng): seems like this should be able to move to OnGotResolvedURL().
  ApplicationLoader* loader = GetLoaderForURL(params->target().url());
  if (loader) {
    GURL url = params->target().url();
    mojom::ShellClientRequest request;
    // TODO(beng): move this to OnGotResolvedURL & read from manifest.
    std::string application_name = url.spec();
    ApplicationInstance* instance = CreateAndConnectToInstance(
        std::move(params), nullptr, nullptr, application_name, &request);
    loader->Load(url, std::move(request));
    instance->RunConnectCallback();
    return;
  }

  std::string url = params->target().url().spec();
  shell_resolver_->ResolveMojoURL(
      url,
      base::Bind(&ApplicationManager::OnGotResolvedURL,
                  weak_ptr_factory_.GetWeakPtr(), base::Passed(&params)));
}

void ApplicationManager::SetLoaderForURL(scoped_ptr<ApplicationLoader> loader,
                                         const GURL& url) {
  URLToLoaderMap::iterator it = url_to_loader_.find(url);
  if (it != url_to_loader_.end())
    delete it->second;
  url_to_loader_[url] = loader.release();
}

void ApplicationManager::TerminateShellConnections() {
  STLDeleteValues(&identity_to_instance_);
}

void ApplicationManager::OnApplicationInstanceError(
    ApplicationInstance* instance) {
  // Called from ~ApplicationInstance, so we do not need to call Destroy here.
  const Identity identity = instance->identity();
  base::Closure on_application_end = instance->on_application_end();
  // Remove the shell.
  auto it = identity_to_instance_.find(identity);
  DCHECK(it != identity_to_instance_.end());
  int id = instance->id();
  delete it->second;
  identity_to_instance_.erase(it);
  listeners_.ForAllPtrs(
      [this, id](mojom::ApplicationManagerListener* listener) {
        listener->ApplicationInstanceDestroyed(id);
      });
  if (!on_application_end.is_null())
    on_application_end.Run();
}

ApplicationInstance* ApplicationManager::GetApplicationInstance(
    const Identity& identity) const {
  const auto& it = identity_to_instance_.find(identity);
  return it != identity_to_instance_.end() ? it->second : nullptr;
}

void ApplicationManager::ApplicationPIDAvailable(
    uint32_t id,
    base::ProcessId pid) {
  for (auto& instance : identity_to_instance_) {
    if (instance.second->id() == id) {
      instance.second->set_pid(pid);
      break;
    }
  }
  listeners_.ForAllPtrs(
      [this, id, pid](mojom::ApplicationManagerListener* listener) {
        listener->ApplicationPIDAvailable(id, pid);
      });
}

////////////////////////////////////////////////////////////////////////////////
// ApplicationManager, ShellClient implementation:

bool ApplicationManager::AcceptConnection(Connection* connection) {
  connection->AddInterface<mojom::ApplicationManager>(this);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ApplicationManager, InterfaceFactory<mojom::ApplicationManager>
//     implementation:

void ApplicationManager::Create(
    Connection* connection,
    mojom::ApplicationManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// ApplicationManager, mojom::ApplicationManager implemetation:

void ApplicationManager::CreateInstanceForHandle(
    ScopedHandle channel,
    const String& url,
    mojom::CapabilityFilterPtr filter,
    mojom::PIDReceiverRequest pid_receiver) {
  // We don't call ConnectToClient() here since the instance was created
  // manually by other code, not in response to a Connect() request. The newly
  // created instance is identified by |url| and may be subsequently reached by
  // client code using this identity.
  CapabilityFilter local_filter = filter->filter.To<CapabilityFilter>();
  Identity target_id(url.To<GURL>(), std::string(), local_filter);
  mojom::ShellClientRequest request;
  // TODO(beng): do better than url.spec() for application name.
  ApplicationInstance* instance = CreateInstance(
      target_id, EmptyConnectCallback(), base::Closure(),
      url, &request);
  instance->BindPIDReceiver(std::move(pid_receiver));
  scoped_ptr<NativeRunner> runner =
      native_runner_factory_->Create(base::FilePath());
  runner->InitHost(std::move(channel), std::move(request));
  instance->SetNativeRunner(runner.get());
  native_runners_.push_back(std::move(runner));
}

void ApplicationManager::AddListener(
    mojom::ApplicationManagerListenerPtr listener) {
  Array<mojom::ApplicationInfoPtr> applications;
  for (auto& entry : identity_to_instance_)
    applications.push_back(CreateApplicationInfoForInstance(entry.second));
  listener->SetRunningApplications(std::move(applications));

  listeners_.AddInterfacePtr(std::move(listener));
}

////////////////////////////////////////////////////////////////////////////////
// ApplicationManager, private:

bool ApplicationManager::ConnectToRunningApplication(
    scoped_ptr<ConnectToApplicationParams>* params) {
  ApplicationInstance* instance = GetApplicationInstance((*params)->target());
  if (!instance)
    return false;

  // TODO(beng): CHECK() that the target URL is already in the application
  //             catalog.
  instance->ConnectToClient(std::move(*params));
  return true;
}

ApplicationInstance* ApplicationManager::CreateAndConnectToInstance(
    scoped_ptr<ConnectToApplicationParams> params,
    Identity* source,
    Identity* target,
    const std::string& application_name,
    mojom::ShellClientRequest* request) {
  if (source)
    *source = params->source();
  if (target)
    *target = params->target();
  ApplicationInstance* instance = CreateInstance(
      params->target(), params->connect_callback(),
      params->on_application_end(),
      application_name,
      request);
  params->set_connect_callback(EmptyConnectCallback());
  instance->ConnectToClient(std::move(params));
  return instance;
}

ApplicationInstance* ApplicationManager::CreateInstance(
    const Identity& target_id,
    const mojom::Shell::ConnectToApplicationCallback& connect_callback,
    const base::Closure& on_application_end,
    const String& application_name,
    mojom::ShellClientRequest* request) {
  mojom::ShellClientPtr shell_client;
  *request = GetProxy(&shell_client);
  ApplicationInstance* instance = new ApplicationInstance(
      std::move(shell_client), this, target_id, connect_callback,
      on_application_end, application_name);
  DCHECK(identity_to_instance_.find(target_id) ==
         identity_to_instance_.end());
  identity_to_instance_[target_id] = instance;
  mojom::ApplicationInfoPtr application_info =
      CreateApplicationInfoForInstance(instance);
  listeners_.ForAllPtrs(
      [this, &application_info](mojom::ApplicationManagerListener* listener) {
        listener->ApplicationInstanceCreated(application_info.Clone());
      });
  instance->InitializeApplication();
  return instance;
}

void ApplicationManager::CreateShellClient(
    const Identity& source,
    const Identity& shell_client_factory,
    const GURL& url,
    mojom::ShellClientRequest request) {
  mojom::ShellClientFactory* factory =
      GetShellClientFactory(shell_client_factory, source);
  factory->CreateShellClient(std::move(request), url.spec());
}

mojom::ShellClientFactory* ApplicationManager::GetShellClientFactory(
    const Identity& shell_client_factory_identity,
    const Identity& source_identity) {
  auto it = shell_client_factories_.find(shell_client_factory_identity);
  if (it != shell_client_factories_.end())
    return it->second.get();

  mojom::ShellClientFactoryPtr factory;
  // TODO(beng): we should forward the original source identity!
  ConnectToInterface(this, source_identity, shell_client_factory_identity.url(),
                     &factory);
  mojom::ShellClientFactory* factory_interface = factory.get();
  factory.set_connection_error_handler(
      base::Bind(&ApplicationManager::OnShellClientFactoryLost,
                 weak_ptr_factory_.GetWeakPtr(),
                 shell_client_factory_identity));
  shell_client_factories_[shell_client_factory_identity] = std::move(factory);
  return factory_interface;
}

void ApplicationManager::OnShellClientFactoryLost(const Identity& which) {
  // Remove the mapping.
  auto it = shell_client_factories_.find(which);
  DCHECK(it != shell_client_factories_.end());
  shell_client_factories_.erase(it);
}

void ApplicationManager::OnGotResolvedURL(
    scoped_ptr<ConnectToApplicationParams> params,
    const String& resolved_url,
    const String& file_url,
    const String& application_name,
    mojom::CapabilityFilterPtr base_filter) {
  // It's possible that when this manifest request was issued, another one was
  // already in-progress and completed by the time this one did, and so the
  // requested application may already be running.
  if (ConnectToRunningApplication(&params))
    return;

  GURL resolved_gurl = resolved_url.To<GURL>();
  if (params->target().url().spec() != resolved_url) {
    CapabilityFilter capability_filter = GetPermissiveCapabilityFilter();
    if (!base_filter.is_null())
      capability_filter = base_filter->filter.To<CapabilityFilter>();

    // TODO(beng): For now, we just use the legacy PackageManagerImpl to manage
    //             the ShellClientFactory connection. Once we get rid of the
    //             non-remote package manager path we will have to fold this in
    //             here.
    Identity source, target;
    mojom::ShellClientRequest request;
    ApplicationInstance* instance = CreateAndConnectToInstance(
        std::move(params), &source, &target, application_name, &request);
    CreateShellClient(source, Identity(resolved_gurl, target.qualifier(),
                      capability_filter), target.url(), std::move(request));
    instance->RunConnectCallback();
    return;
  }
  CreateAndRunLocalApplication(std::move(params), application_name,
                               file_url.To<GURL>());
}

void ApplicationManager::CreateAndRunLocalApplication(
    scoped_ptr<ConnectToApplicationParams> params,
    const String& application_name,
    const GURL& file_url) {
  Identity source, target;
  mojom::ShellClientRequest request;
  ApplicationInstance* instance = CreateAndConnectToInstance(
      std::move(params), &source, &target, application_name, &request);

  bool start_sandboxed = false;
  RunNativeApplication(std::move(request), start_sandboxed, instance,
                       util::UrlToFilePath(file_url));
  instance->RunConnectCallback();
}

void ApplicationManager::RunNativeApplication(
    InterfaceRequest<mojom::ShellClient> request,
    bool start_sandboxed,
    ApplicationInstance* instance,
    const base::FilePath& path) {
  DCHECK(request.is_pending());

  TRACE_EVENT1("mojo_shell", "ApplicationManager::RunNativeApplication", "path",
               path.AsUTF8Unsafe());
  scoped_ptr<NativeRunner> runner = native_runner_factory_->Create(path);
  runner->Start(path, start_sandboxed, std::move(request),
                base::Bind(&ApplicationManager::ApplicationPIDAvailable,
                           weak_ptr_factory_.GetWeakPtr(), instance->id()),
                base::Bind(&ApplicationManager::CleanupRunner,
                           weak_ptr_factory_.GetWeakPtr(), runner.get()));
  instance->SetNativeRunner(runner.get());
  native_runners_.push_back(std::move(runner));
}

ApplicationLoader* ApplicationManager::GetLoaderForURL(const GURL& url) {
  auto url_it = url_to_loader_.find(url);
  if (url_it != url_to_loader_.end())
    return url_it->second;
  return default_loader_.get();
}

void ApplicationManager::CleanupRunner(NativeRunner* runner) {
  for (auto it = native_runners_.begin(); it != native_runners_.end(); ++it) {
    if (it->get() == runner) {
      native_runners_.erase(it);
      return;
    }
  }
}

mojom::ApplicationInfoPtr ApplicationManager::CreateApplicationInfoForInstance(
    ApplicationInstance* instance) const {
  mojom::ApplicationInfoPtr info(mojom::ApplicationInfo::New());
  info->id = instance->id();
  info->url = instance->identity().url().spec();
  info->qualifier = instance->identity().qualifier();
  info->name = instance->application_name();
  if (instance->identity().url().spec() == "mojo://shell/")
    info->pid = base::Process::Current().Pid();
  else
    info->pid = instance->pid();
  return info;
}

}  // namespace shell
}  // namespace mojo
