// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/services/package_manager/loader.h"
#include "mojo/shell/connect_util.h"
#include "mojo/shell/public/cpp/connect.h"
#include "mojo/shell/public/cpp/names.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"
#include "mojo/util/filename_util.h"

namespace mojo {
namespace shell {
namespace {
const char kPackageManagerName[] = "mojo:package_manager";

void EmptyResolverCallback(const String& resolved_name,
                           const String& resolved_qualifier,
                           mojom::CapabilityFilterPtr base_filter,
                           const String& file_url) {}

}

// Encapsulates a connection to an instance of an application, tracked by the
// shell's Shell.
class Shell::Instance : public mojom::Connector,
                        public mojom::PIDReceiver,
                        public ShellClient,
                        public InterfaceFactory<mojom::Shell>,
                        public mojom::Shell {
 public:
  Instance(mojom::ShellClientPtr shell_client,
           mojo::shell::Shell* shell,
           const Identity& identity)
    : shell_(shell),
      id_(GenerateUniqueID()),
      identity_(identity),
      allow_any_application_(identity.filter().size() == 1 &&
      identity.filter().count("*") == 1),
      shell_client_(std::move(shell_client)),
      pid_receiver_binding_(this),
      weak_factory_(this) {
    if (identity_.name() == "mojo:shell" ||
        shell_->GetLoaderForName(identity_.name())) {
      pid_ = base::Process::Current().Pid();
    }
    DCHECK_NE(kInvalidApplicationID, id_);
  }

  ~Instance() override {}

  void InitializeClient() {
    shell_client_->Initialize(connectors_.CreateInterfacePtrAndBind(this),
                              identity_.name(), identity_.user_id(), id_);
    connectors_.set_connection_error_handler(
        base::Bind(&mojo::shell::Shell::OnInstanceError,
                   base::Unretained(shell_), base::Unretained(this)));
  }

  void ConnectToClient(scoped_ptr<ConnectParams> params) {
    params->connect_callback().Run(identity_.user_id(), id_);
    AllowedInterfaces interfaces;
    interfaces.insert("*");
    if (!params->source().is_null())
      interfaces = GetAllowedInterfaces(params->source().filter(), identity_);

    Instance* source = shell_->GetExistingInstance(params->source());
    uint32_t source_id = source ? source->id() : kInvalidApplicationID;
    shell_client_->AcceptConnection(
        params->source().name(), params->source().user_id(), source_id,
        params->TakeRemoteInterfaces(), params->TakeLocalInterfaces(),
        Array<String>::From(interfaces), params->target().name());
  }

  scoped_ptr<NativeRunner> StartWithFileURL(const GURL& file_url,
                                            mojom::ShellClientRequest request,
                                            bool start_sandboxed,
                                            NativeRunnerFactory* factory) {
    base::FilePath path = util::UrlToFilePath(file_url);
    scoped_ptr<NativeRunner> runner = factory->Create(path);
    runner_ = runner.get();
    runner_->Start(path, identity_, start_sandboxed, std::move(request),
                   base::Bind(&Instance::PIDAvailable,
                              weak_factory_.GetWeakPtr()),
                   base::Bind(&mojo::shell::Shell::CleanupRunner,
                              shell_->weak_ptr_factory_.GetWeakPtr(),
                              runner_));
    return runner;
  }

  scoped_ptr<NativeRunner> StartWithFactory(
      mojom::ShellClientFactoryPtr shell_client_factory,
      const String& name,
      mojom::ShellClientRequest request,
      mojom::PIDReceiverRequest pid_receiver_request,
      NativeRunnerFactory* factory) {
    pid_receiver_binding_.Bind(std::move(pid_receiver_request));
    scoped_ptr<NativeRunner> runner = factory->Create(base::FilePath());
    runner_ = runner.get();
    runner_->InitHost(std::move(shell_client_factory), name,
                      std::move(request));
    return runner;
  }

  mojom::InstanceInfoPtr CreateInstanceInfo() const {
    mojom::InstanceInfoPtr info(mojom::InstanceInfo::New());
    info->id = id_;
    info->name = identity_.name();
    info->qualifier = identity_.qualifier();
    info->pid = pid_;
    return info;
  }

  const Identity& identity() const { return identity_; }
  uint32_t id() const { return id_; }

  // ShellClient:
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<mojom::Shell>(this);
    return true;
  }

 private:
  // mojom::Connector implementation:
  void Connect(const String& app_name,
               const String& user_id,
               shell::mojom::InterfaceProviderRequest remote_interfaces,
               shell::mojom::InterfaceProviderPtr local_interfaces,
               const ConnectCallback& callback) override {
    // TODO(beng): Might not want to CHECK here, but rather run the callback
    // with an error code.
    CHECK(base::IsValidGUID(user_id));

    // TODO(beng): perform checking on policy of whether this instance is
    // allowed to pass different user_ids.
    if (!IsValidName(app_name)) {
      LOG(ERROR) << "Error: invalid Name: " << app_name;
      callback.Run(mojom::kInheritUserID, kInvalidApplicationID);
      return;
    }
    if (allow_any_application_ ||
        identity_.filter().find(app_name) != identity_.filter().end()) {
      scoped_ptr<ConnectParams> params(new ConnectParams);
      params->set_source(identity_);
      params->set_target(Identity(app_name, std::string(), user_id));
      params->set_remote_interfaces(std::move(remote_interfaces));
      params->set_local_interfaces(std::move(local_interfaces));
      params->set_connect_callback(callback);
      shell_->Connect(std::move(params));
    }
    else {
      LOG(WARNING) << "CapabilityFilter prevented connection from: " <<
          identity_.name() << " to: " << app_name;
      callback.Run(mojom::kInheritUserID, kInvalidApplicationID);
    }
  }
  void Clone(mojom::ConnectorRequest request) override {
    connectors_.AddBinding(this, std::move(request));
  }

  // mojom::PIDReceiver:
  void SetPID(uint32_t pid) override {
    PIDAvailable(pid);
  }

  // InterfaceFactory<mojom::Shell>:
  void Create(Connection* connection,
              mojom::ShellRequest request) override {
    shell_bindings_.AddBinding(this, std::move(request));
  }

  // mojom::Shell implementation:
  void CreateInstanceForFactory(
      mojom::ShellClientFactoryPtr factory,
      const String& name,
      const String& user_id,
      mojom::CapabilityFilterPtr filter,
      mojom::PIDReceiverRequest pid_receiver) override {
    // TODO(beng): perform checking on policy of whether this instance is
    // allowed to pass different user_ids.
    std::string user_id_string = user_id;
    if (user_id_string == mojom::kInheritUserID)
      user_id_string = identity_.user_id();
    shell_->CreateInstanceForFactory(std::move(factory), name, user_id_string,
                                     std::move(filter),
                                     std::move(pid_receiver));
  }
  void AddInstanceListener(mojom::InstanceListenerPtr listener) override {
    // TODO(beng): this should only track the instances matching this user, and
    // root.
    shell_->AddInstanceListener(std::move(listener));
  }

  uint32_t GenerateUniqueID() const {
    static uint32_t id = kInvalidApplicationID;
    ++id;
    CHECK_NE(kInvalidApplicationID, id);
    return id;
  }

  void PIDAvailable(base::ProcessId pid) {
    pid_ = pid;
    shell_->NotifyPIDAvailable(id_, pid_);
  }

  mojo::shell::Shell* const shell_;
  // An id that identifies this instance. Distinct from pid, as a single process
  // may vend multiple application instances, and this object may exist before a
  // process is launched.
  const uint32_t id_;
  const Identity identity_;
  const bool allow_any_application_;
  mojom::ShellClientPtr shell_client_;
  Binding<mojom::PIDReceiver> pid_receiver_binding_;
  BindingSet<mojom::Connector> connectors_;
  BindingSet<mojom::Shell> shell_bindings_;
  NativeRunner* runner_ = nullptr;
  base::ProcessId pid_ = base::kNullProcessId;
  base::WeakPtrFactory<Instance> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

// static
Shell::TestAPI::TestAPI(Shell* shell) : shell_(shell) {}
Shell::TestAPI::~TestAPI() {}

bool Shell::TestAPI::HasRunningInstanceForName(const std::string& name) const {
  return shell_->identity_to_instance_.find(Identity(name)) !=
         shell_->identity_to_instance_.end();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

Shell::Shell(scoped_ptr<NativeRunnerFactory> native_runner_factory,
             base::TaskRunner* file_task_runner,
             scoped_ptr<package_manager::ApplicationCatalogStore> app_catalog)
    : file_task_runner_(file_task_runner),
      native_runner_factory_(std::move(native_runner_factory)),
      weak_ptr_factory_(this) {
  mojom::ShellClientRequest request;
  CreateInstance(CreateShellIdentity(), &request);
  shell_connection_.reset(new ShellConnection(this, std::move(request)));

  InitPackageManager(std::move(app_catalog));
}

Shell::~Shell() {
  TerminateShellConnections();
  STLDeleteValues(&name_to_loader_);
  for (auto& runner : native_runners_)
    runner.reset();
}

void Shell::SetInstanceQuitCallback(
    base::Callback<void(const Identity&)> callback) {
  instance_quit_callback_ = callback;
}

void Shell::Connect(scoped_ptr<ConnectParams> params) {
  TRACE_EVENT_INSTANT1("mojo_shell", "Shell::Connect",
                       TRACE_EVENT_SCOPE_THREAD, "original_name",
                       params->target().name());
  DCHECK(IsValidName(params->target().name()));

  if (params->target().user_id() == mojom::kInheritUserID) {
    Instance* source = GetExistingInstance(params->source());
    Identity target = params->target();
    // TODO(beng): we should CHECK source.
    target.set_user_id(source ? source->identity().user_id()
                              : mojom::kRootUserID);
    params->set_target(target);
  }

  // Connect to an existing matching instance, if possible.
  if (ConnectToExistingInstance(&params))
    return;

  std::string name = params->target().name();
  shell_resolver_->ResolveMojoName(
      name,
      base::Bind(&Shell::OnGotResolvedName,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&params)));
}

mojom::ShellClientRequest Shell::InitInstanceForEmbedder(
    const std::string& name) {
  DCHECK(!embedder_instance_);

  mojo::shell::Identity target(name, std::string(), mojom::kRootUserID);
  target.set_filter(GetPermissiveCapabilityFilter());
  DCHECK(!GetExistingInstance(target));

  mojom::ShellClientRequest request;
  embedder_instance_ = CreateInstance(target, &request);
  DCHECK(embedder_instance_);

  return request;
}

void Shell::SetLoaderForName(scoped_ptr<Loader> loader,
                             const std::string& name) {
  NameToLoaderMap::iterator it = name_to_loader_.find(name);
  if (it != name_to_loader_.end())
    delete it->second;
  name_to_loader_[name] = loader.release();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, ShellClient implementation:

bool Shell::AcceptConnection(Connection* connection) {
  // The only interface we expose is mojom::Shell, and access to this interface
  // is brokered by a policy specific to each caller, managed by the caller's
  // instance. Here we look to see who's calling, and forward to the caller's
  // instance to continue.
  uint32_t caller_instance_id = mojom::Connector::kInvalidApplicationID;
  CHECK(connection->GetRemoteApplicationID(&caller_instance_id));
  Instance* instance = nullptr;
  for (const auto& entry : identity_to_instance_) {
    if (entry.second->id() == caller_instance_id) {
      instance = entry.second;
      break;
    }
  }
  DCHECK(instance);
  return instance->AcceptConnection(connection);
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

void Shell::InitPackageManager(
    scoped_ptr<package_manager::ApplicationCatalogStore> app_catalog) {
  scoped_ptr<Loader> loader(
      new package_manager::Loader(file_task_runner_, std::move(app_catalog)));
  Loader* loader_raw = loader.get();
  std::string name = kPackageManagerName;
  SetLoaderForName(std::move(loader), name);

  mojom::ShellClientRequest request;
  CreateInstance(Identity(name), &request);
  loader_raw->Load(name, std::move(request));

  ConnectToInterface(this, CreateShellIdentity(), name, &shell_resolver_);

  // Seed the catalog with manifest info for the shell & package manager.
  if (file_task_runner_) {
    shell_resolver_->ResolveMojoName(name, base::Bind(&EmptyResolverCallback));
    shell_resolver_->ResolveMojoName("mojo:shell",
                                     base::Bind(&EmptyResolverCallback));
  }
}

void Shell::TerminateShellConnections() {
  STLDeleteValues(&identity_to_instance_);
}

void Shell::OnInstanceError(Instance* instance) {
  const Identity identity = instance->identity();
  // Remove the shell.
  auto it = identity_to_instance_.find(identity);
  DCHECK(it != identity_to_instance_.end());
  int id = instance->id();
  delete it->second;
  identity_to_instance_.erase(it);
  instance_listeners_.ForAllPtrs([this, id](mojom::InstanceListener* listener) {
                                   listener->InstanceDestroyed(id);
                                 });
  if (!instance_quit_callback_.is_null())
    instance_quit_callback_.Run(identity);
}

Shell::Instance* Shell::GetExistingInstance(const Identity& identity) const {
  const auto& it = identity_to_instance_.find(identity);
  return it != identity_to_instance_.end() ? it->second : nullptr;
}

void Shell::NotifyPIDAvailable(uint32_t id, base::ProcessId pid) {
  instance_listeners_.ForAllPtrs([id, pid](mojom::InstanceListener* listener) {
                                   listener->InstancePIDAvailable(id, pid);
                                 });
}

bool Shell::ConnectToExistingInstance(scoped_ptr<ConnectParams>* params) {
  Instance* instance = GetExistingInstance((*params)->target());
  if (!instance) {
    Identity root_identity = (*params)->target();
    root_identity.set_user_id(mojom::kRootUserID);
    instance = GetExistingInstance(root_identity);
    if (!instance) return false;
  }
  instance->ConnectToClient(std::move(*params));
  return true;
}

Shell::Instance* Shell::CreateInstance(const Identity& target_id,
                                       mojom::ShellClientRequest* request) {
  mojom::ShellClientPtr shell_client;
  *request = GetProxy(&shell_client);
  Instance* instance = new Instance(std::move(shell_client), this, target_id);
  DCHECK(identity_to_instance_.find(target_id) ==
         identity_to_instance_.end());
  identity_to_instance_[target_id] = instance;
  mojom::InstanceInfoPtr info = instance->CreateInstanceInfo();
  instance_listeners_.ForAllPtrs(
      [this, &info](mojom::InstanceListener* listener) {
        listener->InstanceCreated(info.Clone());
      });
  instance->InitializeClient();
  return instance;
}

void Shell::CreateInstanceForFactory(
    mojom::ShellClientFactoryPtr factory,
    const std::string& name,
    const std::string& user_id,
    mojom::CapabilityFilterPtr filter,
    mojom::PIDReceiverRequest pid_receiver) {
  DCHECK(user_id != mojom::kInheritUserID);
  // We don't call ConnectToClient() here since the instance was created
  // manually by other code, not in response to a Connect() request. The newly
  // created instance is identified by |name| and may be subsequently reached by
  // client code using this identity.
  Identity target_id(name, std::string(), mojom::kRootUserID);
  target_id.set_filter(filter->filter.To<CapabilityFilter>());
  mojom::ShellClientRequest request;
  Instance* instance = CreateInstance(target_id, &request);
  native_runners_.push_back(
      instance->StartWithFactory(std::move(factory), name, std::move(request),
                                 std::move(pid_receiver),
                                 native_runner_factory_.get()));
}

void Shell::AddInstanceListener(mojom::InstanceListenerPtr listener) {
  // TODO(beng): filter instances provided by those visible to this client.
  Array<mojom::InstanceInfoPtr> instances;
  for (auto& instance : identity_to_instance_)
    instances.push_back(instance.second->CreateInstanceInfo());
  listener->SetExistingInstances(std::move(instances));

  instance_listeners_.AddInterfacePtr(std::move(listener));
}

void Shell::CreateShellClient(const Identity& source,
                              const Identity& shell_client_factory,
                              const std::string& name,
                              mojom::ShellClientRequest request) {
  mojom::ShellClientFactory* factory =
      GetShellClientFactory(shell_client_factory, source);
  factory->CreateShellClient(std::move(request), name);
}

mojom::ShellClientFactory* Shell::GetShellClientFactory(
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
      base::Bind(&Shell::OnShellClientFactoryLost,
                 weak_ptr_factory_.GetWeakPtr(),
                 shell_client_factory_identity));
  shell_client_factories_[shell_client_factory_identity] = std::move(factory);
  return factory_interface;
}

void Shell::OnShellClientFactoryLost(const Identity& which) {
  // Remove the mapping.
  auto it = shell_client_factories_.find(which);
  DCHECK(it != shell_client_factories_.end());
  shell_client_factories_.erase(it);
}

void Shell::OnGotResolvedName(scoped_ptr<ConnectParams> params,
                              const String& resolved_name,
                              const String& resolved_qualifier,
                              mojom::CapabilityFilterPtr base_filter,
                              const String& file_url) {
  std::string qualifier = params->target().qualifier();
  if (qualifier == GetNamePath(params->target().name()))
    qualifier = resolved_qualifier;
  Identity target(params->target().name(), qualifier,
                  params->target().user_id());
  params->set_target(target);

  // It's possible that when this manifest request was issued, another one was
  // already in-progress and completed by the time this one did, and so the
  // requested application may already be running.
  if (ConnectToExistingInstance(&params))
    return;

  Identity source = params->source();
  // |base_filter| can be null when there is no manifest, e.g. for URL types
  // not resolvable by the resolver.
  CapabilityFilter filter = GetPermissiveCapabilityFilter();
  if (!base_filter.is_null())
    filter = base_filter->filter.To<CapabilityFilter>();
  target.set_filter(filter);

  mojom::ShellClientRequest request;
  Instance* instance = CreateInstance(target, &request);
  instance->ConnectToClient(std::move(params));

  if (LoadWithLoader(target, &request))
    return;

  CHECK(!file_url.is_null() && !base_filter.is_null());

  if (target.name() != resolved_name) {
    // In cases where a package alias is resolved, we have to use the qualifier
    // from the original request rather than for the package itself, which will
    // always be the same.
    CreateShellClient(
        source, Identity(resolved_name, resolved_qualifier, target.user_id()),
        target.name(), std::move(request));
  } else {
    bool start_sandboxed = false;
    native_runners_.push_back(
        instance->StartWithFileURL(file_url.To<GURL>(), std::move(request),
                                   start_sandboxed,
                                   native_runner_factory_.get()));
  }
}

bool Shell::LoadWithLoader(const Identity& target,
                           mojom::ShellClientRequest* request) {
  Loader* loader = GetLoaderForName(target.name());
  if (!loader)
    return false;
  loader->Load(target.name(), std::move(*request));
  return true;
}

Loader* Shell::GetLoaderForName(const std::string& name) {
  auto name_it = name_to_loader_.find(name);
  if (name_it != name_to_loader_.end())
    return name_it->second;
  return default_loader_.get();
}

void Shell::CleanupRunner(NativeRunner* runner) {
  for (auto it = native_runners_.begin(); it != native_runners_.end(); ++it) {
    if (it->get() == runner) {
      native_runners_.erase(it);
      return;
    }
  }
}

}  // namespace shell
}  // namespace mojo
