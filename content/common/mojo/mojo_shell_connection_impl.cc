// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/mojo_shell_connection_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "content/common/mojo/embedded_application_runner.h"
#include "content/public/common/content_switches.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "services/shell/public/cpp/shell_client.h"
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

bool IsRunningInMojoShell() {
  return mojo_shell_connection_factory ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             mojo::edk::PlatformChannelPair::kMojoPlatformChannelHandleSwitch);
}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnection, public:

// static
void MojoShellConnection::SetFactoryForTest(Factory* factory) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  mojo_shell_connection_factory = factory;
}

// static
MojoShellConnection* MojoShellConnection::Get() {
  return lazy_tls_ptr.Pointer()->Get();
}

// static
void MojoShellConnection::Create(shell::mojom::ShellClientRequest request,
                                 bool is_external) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  std::unique_ptr<MojoShellConnection> connection(
      MojoShellConnectionImpl::CreateInstance(std::move(request), is_external));
  lazy_tls_ptr.Pointer()->Set(connection.release());
}

// static
std::unique_ptr<MojoShellConnection> MojoShellConnection::CreateInstance(
    shell::mojom::ShellClientRequest request) {
  return MojoShellConnectionImpl::CreateInstance(std::move(request), false);
}

// static
void MojoShellConnection::Destroy() {
  // This joins the shell controller thread.
  delete Get();
  lazy_tls_ptr.Pointer()->Set(nullptr);
}

MojoShellConnection::~MojoShellConnection() {}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, public:

// static
bool MojoShellConnectionImpl::CreateUsingFactory() {
  if (mojo_shell_connection_factory) {
    DCHECK(!lazy_tls_ptr.Pointer()->Get());
    mojo_shell_connection_factory->Run();
    DCHECK(lazy_tls_ptr.Pointer()->Get());
    return true;
  }
  return false;
}

// static
void MojoShellConnectionImpl::Create() {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  MojoShellConnectionImpl* connection =
      new MojoShellConnectionImpl(true /* external */);
  lazy_tls_ptr.Pointer()->Set(connection);
}

// static
std::unique_ptr<MojoShellConnection> MojoShellConnectionImpl::CreateInstance(
    shell::mojom::ShellClientRequest request,
    bool is_external) {
  MojoShellConnectionImpl* connection =
      new MojoShellConnectionImpl(is_external);
  connection->shell_connection_.reset(
      new shell::ShellConnection(connection, std::move(request)));
  return base::WrapUnique(static_cast<MojoShellConnection*>(connection));
}

// static
MojoShellConnectionImpl* MojoShellConnectionImpl::Get() {
  // Assume that if a mojo_shell_connection_factory was set that it did not
  // create a MojoShellConnectionImpl.
  return static_cast<MojoShellConnectionImpl*>(MojoShellConnection::Get());
}

void MojoShellConnectionImpl::BindToRequestFromCommandLine() {
  DCHECK(!shell_connection_);
  shell_connection_.reset(new shell::ShellConnection(
      this, shell::GetShellClientRequestFromCommandLine()));
}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, private:

MojoShellConnectionImpl::MojoShellConnectionImpl(bool external)
    : external_(external) {}
MojoShellConnectionImpl::~MojoShellConnectionImpl() {}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, shell::ShellClient implementation:

void MojoShellConnectionImpl::Initialize(shell::Connector* connector,
                                         const shell::Identity& identity,
                                         uint32_t id) {
  for (auto& client : embedded_shell_clients_)
    client->Initialize(connector, identity, id);
}

bool MojoShellConnectionImpl::AcceptConnection(shell::Connection* connection) {
  std::string remote_app = connection->GetRemoteIdentity().name();
  if (remote_app == "mojo:shell") {
    // Only expose the SCF interface to the shell.
    connection->AddInterface<shell::mojom::ShellClientFactory>(this);
    return true;
  }

  bool accept = false;
  for (auto& client : embedded_shell_clients_)
    accept |= client->AcceptConnection(connection);

  // Reject all other connections to this application.
  return accept;
}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl,
//     shell::InterfaceFactory<shell::mojom::ShellClientFactory> implementation:

void MojoShellConnectionImpl::Create(
    shell::Connection* connection,
    shell::mojom::ShellClientFactoryRequest request) {
  factory_bindings_.AddBinding(this, std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, shell::mojom::ShellClientFactory implementation:

void MojoShellConnectionImpl::CreateShellClient(
    shell::mojom::ShellClientRequest request,
    const mojo::String& name) {
  auto it = request_handlers_.find(name);
  if (it != request_handlers_.end())
    it->second.Run(std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// MojoShellConnectionImpl, MojoShellConnection implementation:

shell::Connector* MojoShellConnectionImpl::GetConnector() {
  DCHECK(shell_connection_);
  return shell_connection_->connector();
}

const shell::Identity& MojoShellConnectionImpl::GetIdentity() const {
  DCHECK(shell_connection_);
  return shell_connection_->identity();
}

bool MojoShellConnectionImpl::UsingExternalShell() const {
  return external_;
}

void MojoShellConnectionImpl::SetConnectionLostClosure(
    const base::Closure& closure) {
  shell_connection_->SetConnectionLostClosure(closure);
}

void MojoShellConnectionImpl::AddEmbeddedShellClient(
    std::unique_ptr<shell::ShellClient> shell_client) {
  embedded_shell_clients_.push_back(std::move(shell_client));
}

void MojoShellConnectionImpl::AddEmbeddedService(
    const std::string& name,
    const MojoApplicationInfo& info) {
  std::unique_ptr<EmbeddedApplicationRunner> app(
      new EmbeddedApplicationRunner(name, info));
  AddShellClientRequestHandler(
      name, base::Bind(&EmbeddedApplicationRunner::BindShellClientRequest,
                       base::Unretained(app.get())));
  auto result = embedded_apps_.insert(std::make_pair(name, std::move(app)));
  DCHECK(result.second);
}

void MojoShellConnectionImpl::AddShellClientRequestHandler(
    const std::string& name,
    const ShellClientRequestHandler& handler) {
  auto result = request_handlers_.insert(std::make_pair(name, handler));
  DCHECK(result.second);
}

}  // namespace content
