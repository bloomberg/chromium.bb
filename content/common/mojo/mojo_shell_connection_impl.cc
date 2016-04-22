// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/mojo_shell_connection_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "content/public/common/content_switches.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/runner/common/client_util.h"

namespace content {
namespace {

using MojoShellConnectionPtr =
    base::ThreadLocalPointer<MojoShellConnectionImpl>;

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

bool ShouldWaitForShell() {
  return mojo_shell_connection_factory ||
         (IsRunningInMojoShell() &&
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kWaitForMojoShell));
}

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
void MojoShellConnection::Create(shell::mojom::ShellClientRequest request,
                                 bool is_external) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  MojoShellConnectionImpl* connection =
      new MojoShellConnectionImpl(is_external);
  lazy_tls_ptr.Pointer()->Set(connection);
  connection->shell_connection_.reset(
      new shell::ShellConnection(connection, std::move(request)));
  if (is_external)
    connection->WaitForShellIfNecessary();
}

// static
void MojoShellConnection::SetFactoryForTest(Factory* factory) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  mojo_shell_connection_factory = factory;
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
  WaitForShellIfNecessary();
}

MojoShellConnectionImpl::MojoShellConnectionImpl(bool external)
    : external_(external) {}

MojoShellConnectionImpl::~MojoShellConnectionImpl() {
  STLDeleteElements(&listeners_);
}

void MojoShellConnectionImpl::WaitForShellIfNecessary() {
  // TODO(rockot): Remove this. http://crbug.com/594852.
  if (ShouldWaitForShell()) {
    base::RunLoop wait_loop;
    shell_connection_->set_initialize_handler(wait_loop.QuitClosure());
    wait_loop.Run();
  }
}

void MojoShellConnectionImpl::Initialize(shell::Connector* connector,
                                         const shell::Identity& identity,
                                         uint32_t id) {}

bool MojoShellConnectionImpl::AcceptConnection(shell::Connection* connection) {
  bool found = false;
  for (auto listener : listeners_)
    found |= listener->AcceptConnection(connection);
  return found;
}

shell::Connector* MojoShellConnectionImpl::GetConnector() {
  DCHECK(shell_connection_);
  return shell_connection_->connector();
}

bool MojoShellConnectionImpl::UsingExternalShell() const {
  return external_;
}

void MojoShellConnectionImpl::SetConnectionLostClosure(
    const base::Closure& closure) {
  shell_connection_->set_connection_lost_closure(closure);
}

void MojoShellConnectionImpl::AddListener(std::unique_ptr<Listener> listener) {
  DCHECK(std::find(listeners_.begin(), listeners_.end(), listener.get()) ==
         listeners_.end());
  listeners_.push_back(listener.release());
}

std::unique_ptr<MojoShellConnection::Listener>
MojoShellConnectionImpl::RemoveListener(Listener* listener) {
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  DCHECK(it != listeners_.end());
  listeners_.erase(it);
  return base::WrapUnique(listener);
}

// static
MojoShellConnection* MojoShellConnection::Get() {
  return lazy_tls_ptr.Pointer()->Get();
}

// static
void MojoShellConnection::Destroy() {
  // This joins the shell controller thread.
  delete Get();
  lazy_tls_ptr.Pointer()->Set(nullptr);
}

MojoShellConnection::~MojoShellConnection() {}

}  // namespace content
