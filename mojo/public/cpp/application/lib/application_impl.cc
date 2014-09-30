// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_impl.h"

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/lib/service_registry.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/environment/logging.h"

namespace mojo {

class ApplicationImpl::ShellPtrWatcher : public ErrorHandler {
 public:
  ShellPtrWatcher(ApplicationImpl* impl)
    : impl_(impl) {}

  virtual ~ShellPtrWatcher() {}

  virtual void OnConnectionError() override { impl_->OnShellError(); }

 private:
  ApplicationImpl* impl_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(ShellPtrWatcher);
};

ApplicationImpl::ApplicationImpl(ApplicationDelegate* delegate,
                                 ScopedMessagePipeHandle shell_handle)
    : initialized_(false), delegate_(delegate), shell_watch_(nullptr) {
  BindShell(shell_handle.Pass());
}

ApplicationImpl::ApplicationImpl(ApplicationDelegate* delegate,
                                 MojoHandle shell_handle)
    : initialized_(false), delegate_(delegate), shell_watch_(nullptr) {
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
  delete shell_watch_;
}

void ApplicationImpl::Initialize(Array<String> args) {
  MOJO_CHECK(!initialized_);
  initialized_ = true;
  args_ = args.Pass();
  delegate_->Initialize(this);
}

ApplicationConnection* ApplicationImpl::ConnectToApplication(
    const String& application_url) {
  MOJO_CHECK(initialized_);
  ServiceProviderPtr out_service_provider;
  shell_->ConnectToApplication(application_url, Get(&out_service_provider));
  internal::ServiceRegistry* registry = new internal::ServiceRegistry(
      this,
      application_url,
      out_service_provider.Pass());
  if (!delegate_->ConfigureOutgoingConnection(registry)) {
    delete registry;
    return nullptr;
  }
  outgoing_service_registries_.push_back(registry);
  return registry;
}

void ApplicationImpl::BindShell(ScopedMessagePipeHandle shell_handle) {
  shell_watch_ = new ShellPtrWatcher(this);
  shell_.Bind(shell_handle.Pass());
  shell_.set_client(this);
  shell_.set_error_handler(shell_watch_);
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
