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

ApplicationManager::ApplicationManager(bool register_mojo_url_schemes)
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

  InitPackageManager(register_mojo_url_schemes);
}

ApplicationManager::~ApplicationManager() {
  TerminateShellConnections();
  STLDeleteValues(&url_to_loader_);
  for (auto& runner : native_runners_)
    runner.reset();
}

void ApplicationManager::SetInstanceQuitCallback(
    base::Callback<void(const Identity&)> callback) {
  instance_quit_callback_ = callback;
}

void ApplicationManager::Connect(scoped_ptr<ConnectParams> params) {
  TRACE_EVENT_INSTANT1("mojo_shell", "ApplicationManager::Connect",
                       TRACE_EVENT_SCOPE_THREAD, "original_url",
                       params->target().url().spec());
  DCHECK(params->target().url().is_valid());

  // Connect to an existing matching instance, if possible.
  if (ConnectToExistingInstance(&params))
    return;

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
  const Identity identity = instance->identity();
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
  if (!instance_quit_callback_.is_null())
    instance_quit_callback_.Run(identity);
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

void ApplicationManager::Create(Connection* connection,
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
  ApplicationInstance* instance = CreateInstance(target_id, &request);
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

void ApplicationManager::InitPackageManager(bool register_mojo_url_schemes) {
  scoped_ptr<ApplicationLoader> loader(
      new package_manager::Loader(task_runner_, register_mojo_url_schemes));

  mojom::ShellClientRequest request;
  GURL url("mojo://package_manager/");
  CreateInstance(Identity(url), &request);
  loader->Load(url, std::move(request));

  SetLoaderForURL(std::move(loader), url);

  ConnectToInterface(this, CreateShellIdentity(), url, &shell_resolver_);
}

bool ApplicationManager::ConnectToExistingInstance(
    scoped_ptr<ConnectParams>* params) {
  ApplicationInstance* instance = GetApplicationInstance((*params)->target());
  if (!instance)
    return false;
  instance->ConnectToClient(std::move(*params));
  return true;
}

ApplicationInstance* ApplicationManager::CreateInstance(
    const Identity& target_id,
    mojom::ShellClientRequest* request) {
  mojom::ShellClientPtr shell_client;
  *request = GetProxy(&shell_client);
  ApplicationInstance* instance =
      new ApplicationInstance(std::move(shell_client), this, target_id);
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
  ConnectToInterface(this, source_identity, shell_client_factory_identity,
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
    scoped_ptr<ConnectParams> params,
    const String& resolved_url,
    const String& resolved_qualifier,
    mojom::CapabilityFilterPtr base_filter,
    const String& file_url) {
  // It's possible that when this manifest request was issued, another one was
  // already in-progress and completed by the time this one did, and so the
  // requested application may already be running.
  if (ConnectToExistingInstance(&params))
    return;

  Identity source = params->source();
  CapabilityFilter filter = params->target().filter();
  // TODO(beng): this clobbers the filter passed via Connect().
  if (!base_filter.is_null())
    filter = base_filter->filter.To<CapabilityFilter>();
  Identity target(params->target().url(), params->target().qualifier(), filter);

  mojom::ShellClientRequest request;
  ApplicationInstance* instance = CreateInstance(target, &request);
  instance->ConnectToClient(std::move(params));

  if (LoadWithLoader(target, &request))
    return;

  CHECK(!file_url.is_null() && !base_filter.is_null());

  GURL resolved_gurl = resolved_url.To<GURL>();
  if (target.url().spec() != resolved_url) {
    // In cases where a package alias is resolved, we have to use the qualifier
    // from the original request rather than for the package itself, which will
    // always be the same.
    CreateShellClient(source,
                      Identity(resolved_gurl, target.qualifier(), filter),
                      target.url(), std::move(request));
  } else {
    bool start_sandboxed = false;
    base::FilePath path = util::UrlToFilePath(file_url.To<GURL>());
    scoped_ptr<NativeRunner> runner = native_runner_factory_->Create(path);
    runner->Start(path, target, start_sandboxed, std::move(request),
                  base::Bind(&ApplicationManager::ApplicationPIDAvailable,
                             weak_ptr_factory_.GetWeakPtr(), instance->id()),
                  base::Bind(&ApplicationManager::CleanupRunner,
                             weak_ptr_factory_.GetWeakPtr(), runner.get()));
    instance->SetNativeRunner(runner.get());
    native_runners_.push_back(std::move(runner));
  }
}

bool ApplicationManager::LoadWithLoader(const Identity& target,
                                        mojom::ShellClientRequest* request) {
  ApplicationLoader* loader = GetLoaderForURL(target.url());
  if (!loader)
    return false;
  loader->Load(target.url(), std::move(*request));
  return true;
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
  if (instance->identity().url().spec() == "mojo://shell/")
    info->pid = base::Process::Current().Pid();
  else
    info->pid = instance->pid();
  return info;
}

}  // namespace shell
}  // namespace mojo
