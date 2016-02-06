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
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
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
  mojo::InterfaceRequest<mojo::Application> application_request;
  runner_connection_.reset(mojo::shell::RunnerConnection::ConnectToRunner(
      &application_request, std::move(handle)));
  application_impl_.reset(
      new mojo::ApplicationImpl(this, std::move(application_request)));
  application_impl_->WaitForInitialize();
}

void MojoShellConnectionImpl::Initialize(mojo::ApplicationImpl* application) {
  initialized_ = true;
}

bool MojoShellConnectionImpl::AcceptConnection(
    mojo::ApplicationConnection* connection) {
  bool found = false;
  for (auto listener : listeners_)
    found |= listener->AcceptConnection(connection);
  return found;
}

mojo::ApplicationImpl* MojoShellConnectionImpl::GetApplication() {
  DCHECK(initialized_);
  return application_impl_.get();
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
