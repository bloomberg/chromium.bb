// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/mojo_shell_connection_impl.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_local.h"
#include "content/common/mojo/embedded_application_runner.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/runner/common/client_util.h"

namespace content {
namespace {

using MojoShellConnectionPtr =
    base::ThreadLocalPointer<MojoShellConnection>;

// Env is thread local so that aura may be used on multiple threads.
base::LazyInstance<MojoShellConnectionPtr>::Leaky lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

MojoShellConnection::Factory* mojo_shell_connection_factory = nullptr;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnection, public:

// static
void MojoShellConnection::SetForProcess(
    std::unique_ptr<MojoShellConnection> connection) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  lazy_tls_ptr.Pointer()->Set(connection.release());
}

// static
MojoShellConnection* MojoShellConnection::GetForProcess() {
  return lazy_tls_ptr.Pointer()->Get();
}

// static
void MojoShellConnection::DestroyForProcess() {
  // This joins the shell controller thread.
  delete GetForProcess();
  lazy_tls_ptr.Pointer()->Set(nullptr);
}

// static
void MojoShellConnection::SetFactoryForTest(Factory* factory) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  mojo_shell_connection_factory = factory;
}

// static
std::unique_ptr<MojoShellConnection> MojoShellConnection::Create(
    shell::mojom::ServiceRequest request) {
  if (mojo_shell_connection_factory)
    return mojo_shell_connection_factory->Run();
  return base::WrapUnique(new MojoShellConnectionImpl(std::move(request)));
}

MojoShellConnection::~MojoShellConnection() {}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, public:

MojoShellConnectionImpl::MojoShellConnectionImpl(
    shell::mojom::ServiceRequest request)
    : shell_connection_(new shell::ShellConnection(this, std::move(request))) {}

MojoShellConnectionImpl::~MojoShellConnectionImpl() {}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, shell::Service implementation:

void MojoShellConnectionImpl::OnStart(shell::Connector* connector,
                                      const shell::Identity& identity,
                                      uint32_t id) {
  for (auto& client : embedded_services_)
    client->OnStart(connector, identity, id);
}

bool MojoShellConnectionImpl::OnConnect(shell::Connection* connection) {
  std::string remote_app = connection->GetRemoteIdentity().name();
  if (remote_app == "mojo:shell") {
    // Only expose the SCF interface to the shell.
    connection->AddInterface<shell::mojom::ServiceFactory>(this);
    return true;
  }

  bool accept = false;
  for (auto& client : embedded_services_)
    accept |= client->OnConnect(connection);

  // Reject all other connections to this application.
  return accept;
}

shell::InterfaceRegistry*
MojoShellConnectionImpl::GetInterfaceRegistryForConnection() {
  // TODO(beng): This is really horrible since obviously subject to issues
  // of ordering, but is no more horrible than this API is in general.
  shell::InterfaceRegistry* registry = nullptr;
  for (auto& client : embedded_services_) {
    registry = client->GetInterfaceRegistryForConnection();
    if (registry)
      return registry;
  }
  return nullptr;
}

shell::InterfaceProvider*
MojoShellConnectionImpl::GetInterfaceProviderForConnection() {
  // TODO(beng): This is really horrible since obviously subject to issues
  // of ordering, but is no more horrible than this API is in general.
  shell::InterfaceProvider* provider = nullptr;
  for (auto& client : embedded_services_) {
    provider = client->GetInterfaceProviderForConnection();
    if (provider)
      return provider;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl,
//     shell::InterfaceFactory<shell::mojom::ServiceFactory> implementation:

void MojoShellConnectionImpl::Create(
    shell::Connection* connection,
    shell::mojom::ServiceFactoryRequest request) {
  factory_bindings_.AddBinding(this, std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, shell::mojom::ServiceFactory implementation:

void MojoShellConnectionImpl::CreateService(
    shell::mojom::ServiceRequest request,
    const mojo::String& name) {
  auto it = request_handlers_.find(name);
  if (it != request_handlers_.end())
    it->second.Run(std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, MojoShellConnection implementation:

shell::ShellConnection* MojoShellConnectionImpl::GetShellConnection() {
  return shell_connection_.get();
}

shell::Connector* MojoShellConnectionImpl::GetConnector() {
  DCHECK(shell_connection_);
  return shell_connection_->connector();
}

const shell::Identity& MojoShellConnectionImpl::GetIdentity() const {
  DCHECK(shell_connection_);
  return shell_connection_->identity();
}

void MojoShellConnectionImpl::SetConnectionLostClosure(
    const base::Closure& closure) {
  shell_connection_->SetConnectionLostClosure(closure);
}

void MojoShellConnectionImpl::MergeService(
    std::unique_ptr<shell::Service> service) {
  embedded_services_.push_back(service.get());
  owned_services_.push_back(std::move(service));
}

void MojoShellConnectionImpl::MergeService(
    shell::Service* service) {
  embedded_services_.push_back(service);
}

void MojoShellConnectionImpl::AddEmbeddedService(
    const std::string& name,
    const MojoApplicationInfo& info) {
  std::unique_ptr<EmbeddedApplicationRunner> app(
      new EmbeddedApplicationRunner(name, info));
  AddServiceRequestHandler(
      name, base::Bind(&EmbeddedApplicationRunner::BindServiceRequest,
                       base::Unretained(app.get())));
  auto result = embedded_apps_.insert(std::make_pair(name, std::move(app)));
  DCHECK(result.second);
}

void MojoShellConnectionImpl::AddServiceRequestHandler(
    const std::string& name,
    const ServiceRequestHandler& handler) {
  auto result = request_handlers_.insert(std::make_pair(name, handler));
  DCHECK(result.second);
}

}  // namespace content
