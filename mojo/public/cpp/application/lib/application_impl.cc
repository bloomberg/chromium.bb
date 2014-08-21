// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_impl.h"

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/lib/service_registry.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"

namespace mojo {

ApplicationImpl::ShellPtrWatcher::ShellPtrWatcher(ApplicationImpl* impl)
    : impl_(impl) {}

ApplicationImpl::ShellPtrWatcher::~ShellPtrWatcher() {}

void ApplicationImpl::ShellPtrWatcher::OnConnectionError() {
  impl_->OnShellError();
}

ApplicationImpl::ApplicationImpl(ApplicationDelegate* delegate,
                                 ScopedMessagePipeHandle shell_handle)
    : delegate_(delegate), shell_watch_(this) {
  BindShell(shell_handle.Pass());
}

ApplicationImpl::ApplicationImpl(ApplicationDelegate* delegate,
                                 MojoHandle shell_handle)
    : delegate_(delegate), shell_watch_(this) {
  BindShell(MakeScopedHandle(MessagePipeHandle(shell_handle)));
}

void ApplicationImpl::ClearConnections() {
  for (ServiceRegistryList::iterator i(incoming_service_registries_.begin());
      i != incoming_service_registries_.end(); ++i)
    delete *i;
  for (ServiceRegistryList::iterator i(outgoing_service_registries_.begin());
      i != outgoing_service_registries_.end(); ++i)
    delete *i;
  incoming_service_registries_.clear();
  outgoing_service_registries_.clear();
}

ApplicationImpl::~ApplicationImpl() {
  ClearConnections();
}

ApplicationConnection* ApplicationImpl::ConnectToApplication(
    const String& application_url) {
  ServiceProviderPtr out_service_provider;
  shell_->ConnectToApplication(application_url, Get(&out_service_provider));
  internal::ServiceRegistry* registry = new internal::ServiceRegistry(
      this,
      application_url,
      out_service_provider.Pass());
  if (!delegate_->ConfigureOutgoingConnection(registry)) {
    delete registry;
    return NULL;
  }
  outgoing_service_registries_.push_back(registry);
  return registry;
}

void ApplicationImpl::BindShell(ScopedMessagePipeHandle shell_handle) {
  shell_.Bind(shell_handle.Pass());
  shell_.set_client(this);
  shell_.set_error_handler(&shell_watch_);
  delegate_->Initialize(this);
}

void ApplicationImpl::AcceptConnection(const String& requestor_url,
                                       ServiceProviderPtr service_provider) {
  internal::ServiceRegistry* registry = new internal::ServiceRegistry(
      this, requestor_url, service_provider.Pass());
  if (!delegate_->ConfigureIncomingConnection(registry)) {
    delete registry;
    return;
  }
  incoming_service_registries_.push_back(registry);
}

}  // namespace mojo
