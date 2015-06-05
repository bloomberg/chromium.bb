// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/public/cpp/application_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/lib/service_registry.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/environment/logging.h"

namespace mojo {

namespace {

void DefaultTerminationClosure() {
  if (base::MessageLoop::current()->is_running())
    base::MessageLoop::current()->Quit();
}

}  // namespace

ApplicationImpl::ApplicationImpl(ApplicationDelegate* delegate,
                                 InterfaceRequest<Application> request)
    : ApplicationImpl(delegate, request.Pass(),
                      base::Bind(&DefaultTerminationClosure)) {
}

ApplicationImpl::ApplicationImpl(ApplicationDelegate* delegate,
                                 InterfaceRequest<Application> request,
                                 const base::Closure& termination_closure)
    : delegate_(delegate),
      binding_(this, request.Pass()),
      termination_closure_(termination_closure),
      app_lifetime_helper_(this),
      quit_requested_(false),
      weak_factory_(this) {
}

void ApplicationImpl::ClearConnections() {
  for (ServiceRegistryList::iterator i(incoming_service_registries_.begin());
       i != incoming_service_registries_.end();
       ++i)
    delete *i;
  for (ServiceRegistryList::iterator i(outgoing_service_registries_.begin());
       i != outgoing_service_registries_.end();
       ++i)
    delete *i;
  incoming_service_registries_.clear();
  outgoing_service_registries_.clear();
}

ApplicationImpl::~ApplicationImpl() {
  ClearConnections();
  app_lifetime_helper_.ApplicationTerminated();
}

ApplicationConnection* ApplicationImpl::ConnectToApplication(
    mojo::URLRequestPtr request) {
  MOJO_CHECK(shell_);
  ServiceProviderPtr local_services;
  InterfaceRequest<ServiceProvider> local_request = GetProxy(&local_services);
  ServiceProviderPtr remote_services;
  std::string application_url = request->url.To<std::string>();
  shell_->ConnectToApplication(request.Pass(), GetProxy(&remote_services),
                               local_services.Pass());
  internal::ServiceRegistry* registry = new internal::ServiceRegistry(
      this, application_url, application_url, remote_services.Pass(),
      local_request.Pass());
  if (!delegate_->ConfigureOutgoingConnection(registry)) {
    delete registry;
    return nullptr;
  }
  outgoing_service_registries_.push_back(registry);
  return registry;
}

void ApplicationImpl::Initialize(ShellPtr shell, const mojo::String& url) {
  shell_ = shell.Pass();
  shell_.set_error_handler(this);
  url_ = url;
  delegate_->Initialize(this);
}

void ApplicationImpl::WaitForInitialize() {
  if (!shell_)
    binding_.WaitForIncomingMethodCall();
}

void ApplicationImpl::UnbindConnections(
    InterfaceRequest<Application>* application_request,
    ShellPtr* shell) {
  *application_request = binding_.Unbind();
  shell->Bind(shell_.PassInterface());
}

void ApplicationImpl::Terminate() {
  // We can't quit immediately, since there could be in-flight requests from the
  // shell. So check with it first.
  if (shell_) {
    quit_requested_ = true;
    shell_->QuitApplication();
  } else {
    QuitNow();
  }
}

void ApplicationImpl::QuitNow() {
  delegate_->Quit();
  termination_closure_.Run();
}

void ApplicationImpl::AcceptConnection(
    const String& requestor_url,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    const String& url) {
  internal::ServiceRegistry* registry = new internal::ServiceRegistry(
      this, url, requestor_url, exposed_services.Pass(), services.Pass());
  if (!delegate_->ConfigureIncomingConnection(registry)) {
    delete registry;
    return;
  }
  incoming_service_registries_.push_back(registry);

  // If we were quitting because we thought there were no more services for this
  // app in use, then that has changed so cancel the quit request.
  if (quit_requested_)
    quit_requested_ = false;
}

void ApplicationImpl::OnQuitRequested(const Callback<void(bool)>& callback) {
  // If by the time we got the reply from the shell, more requests had come in
  // then we don't want to quit the app anymore so we return false. Otherwise
  // |quit_requested_| is true so we tell the shell to proceed with the quit.
  callback.Run(quit_requested_);
  if (quit_requested_)
    QuitNow();
}

void ApplicationImpl::OnConnectionError() {
  base::WeakPtr<ApplicationImpl> ptr(weak_factory_.GetWeakPtr());
  QuitNow();
  if (!ptr)
    return;
  shell_ = nullptr;
}

}  // namespace mojo
