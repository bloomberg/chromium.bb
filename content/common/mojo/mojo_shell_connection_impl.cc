// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/mojo_shell_connection_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "mojo/converters/network/network_type_converters.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/shell/runner/child/runner_connection.h"

namespace content {
namespace {
using MojoShellConnectionPtr =
    base::ThreadLocalPointer<MojoShellConnectionImpl>;

// Env is thread local so that aura may be used on multiple threads.
base::LazyInstance<MojoShellConnectionPtr>::Leaky lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool IsRunningInMojoShell() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      "mojo-platform-channel-handle");
}

// static
void MojoShellConnectionImpl::Create() {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  MojoShellConnectionImpl* connection = new MojoShellConnectionImpl;
  lazy_tls_ptr.Pointer()->Set(connection);
}

// static
MojoShellConnectionImpl* MojoShellConnectionImpl::Get() {
  return static_cast<MojoShellConnectionImpl*>(MojoShellConnection::Get());
}

void MojoShellConnectionImpl::BindToCommandLinePlatformChannel() {
  DCHECK(IsRunningInMojoShell());
  if (initialized_)
    return;
  WaitForShell(mojo::ScopedMessagePipeHandle());
}

void MojoShellConnectionImpl::BindToMessagePipe(
    mojo::ScopedMessagePipeHandle handle) {
  if (initialized_)
    return;
  WaitForShell(std::move(handle));
}

MojoShellConnectionImpl::MojoShellConnectionImpl() : initialized_(false) {}
MojoShellConnectionImpl::~MojoShellConnectionImpl() {
  STLDeleteElements(&listeners_);
}

void MojoShellConnectionImpl::WaitForShell(
    mojo::ScopedMessagePipeHandle handle) {
  mojo::ShellClientRequest request;
  runner_connection_.reset(mojo::shell::RunnerConnection::ConnectToRunner(
      &request, std::move(handle)));
  shell_connection_.reset(new mojo::ShellConnection(this, std::move(request)));
  shell_connection_->WaitForInitialize();
}

void MojoShellConnectionImpl::Initialize(mojo::Shell* shell,
                                         const std::string& url,
                                         uint32_t id) {
  initialized_ = true;
}

bool MojoShellConnectionImpl::AcceptConnection(mojo::Connection* connection) {
  bool found = false;
  for (auto listener : listeners_)
    found |= listener->AcceptConnection(connection);
  return found;
}

mojo::Shell* MojoShellConnectionImpl::GetShell() {
  DCHECK(initialized_);
  return shell_connection_.get();
}

void MojoShellConnectionImpl::AddListener(Listener* listener) {
  DCHECK(std::find(listeners_.begin(), listeners_.end(), listener) ==
         listeners_.end());
  listeners_.push_back(listener);
}

void MojoShellConnectionImpl::RemoveListener(Listener* listener) {
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  DCHECK(it != listeners_.end());
  listeners_.erase(it);
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
