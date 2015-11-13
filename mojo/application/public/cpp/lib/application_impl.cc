// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/public/cpp/application_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/lib/service_registry.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/environment/logging.h"

namespace mojo {

namespace {

void DefaultTerminationClosure() {
  if (base::MessageLoop::current() &&
      base::MessageLoop::current()->is_running())
    base::MessageLoop::current()->QuitWhenIdle();
}

}  // namespace

ApplicationImpl::ApplicationImpl(ApplicationDelegate* delegate,
                                 InterfaceRequest<Application> request)
    : ApplicationImpl(delegate, request.Pass(),
                      base::Bind(&DefaultTerminationClosure)) {
}

ApplicationImpl::ApplicationImpl(ApplicationDelegate* delegate,
                                 InterfaceRequest<Application> request,
                                 const Closure& termination_closure)
    : delegate_(delegate),
      binding_(this, request.Pass()),
      termination_closure_(termination_closure),
      app_lifetime_helper_(this),
      quit_requested_(false),
      weak_factory_(this) {}

ApplicationImpl::~ApplicationImpl() {
  app_lifetime_helper_.OnQuit();
}

scoped_ptr<ApplicationConnection> ApplicationImpl::ConnectToApplication(
    const std::string& url) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = url;
  return ConnectToApplication(request.Pass());
}

scoped_ptr<ApplicationConnection> ApplicationImpl::ConnectToApplication(
    URLRequestPtr request) {
  return ConnectToApplicationWithCapabilityFilter(request.Pass(), nullptr);
}

scoped_ptr<ApplicationConnection>
    ApplicationImpl::ConnectToApplicationWithCapabilityFilter(
        URLRequestPtr request,
        CapabilityFilterPtr filter) {
  if (!shell_)
    return nullptr;
  ServiceProviderPtr local_services;
  InterfaceRequest<ServiceProvider> local_request = GetProxy(&local_services);
  ServiceProviderPtr remote_services;
  std::string application_url = request->url.To<std::string>();
  // We allow all interfaces on outgoing connections since we are presumably in
  // a position to know who we're talking to.
  // TODO(beng): is this a valid assumption or do we need to figure some way to
  //             filter here too?
  std::set<std::string> allowed;
  allowed.insert("*");
  InterfaceRequest<ServiceProvider> remote_services_proxy =
      GetProxy(&remote_services);
  scoped_ptr<internal::ServiceRegistry> registry(new internal::ServiceRegistry(
      application_url, application_url, remote_services.Pass(),
      local_request.Pass(), allowed));
  shell_->ConnectToApplication(request.Pass(), remote_services_proxy.Pass(),
                               local_services.Pass(), filter.Pass(),
                               registry->GetConnectToApplicationCallback());
  if (!delegate_->ConfigureOutgoingConnection(registry.get()))
    return nullptr;
  return registry.Pass();
}

void ApplicationImpl::WaitForInitialize() {
  DCHECK(!shell_.is_bound());
  binding_.WaitForIncomingMethodCall();
}

void ApplicationImpl::Quit() {
  // We can't quit immediately, since there could be in-flight requests from the
  // shell. So check with it first.
  if (shell_) {
    quit_requested_ = true;
    shell_->QuitApplication();
  } else {
    QuitNow();
  }
}

void ApplicationImpl::Initialize(ShellPtr shell, const mojo::String& url) {
  shell_ = shell.Pass();
  shell_.set_connection_error_handler([this]() { OnConnectionError(); });
  url_ = url;
  delegate_->Initialize(this);
}

void ApplicationImpl::AcceptConnection(
    const String& requestor_url,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    Array<String> allowed_interfaces,
    const String& url) {
  scoped_ptr<ApplicationConnection> registry(new internal::ServiceRegistry(
      url, requestor_url, exposed_services.Pass(), services.Pass(),
      allowed_interfaces.To<std::set<std::string>>()));
  if (!delegate_->ConfigureIncomingConnection(registry.get()))
    return;

  // If we were quitting because we thought there were no more services for this
  // app in use, then that has changed so cancel the quit request.
  if (quit_requested_)
    quit_requested_ = false;

  incoming_connections_.push_back(registry.Pass());
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

  // We give the delegate notice first, since it might want to do something on
  // shell connection errors other than immediate termination of the run
  // loop. The application might want to continue servicing connections other
  // than the one to the shell.
  bool quit_now = delegate_->OnShellConnectionError();
  if (quit_now)
    QuitNow();
  if (!ptr)
    return;
  shell_ = nullptr;
}

void ApplicationImpl::QuitNow() {
  delegate_->Quit();
  termination_closure_.Run();
}

void ApplicationImpl::UnbindConnections(
    InterfaceRequest<Application>* application_request,
    ShellPtr* shell) {
  *application_request = binding_.Unbind();
  shell->Bind(shell_.PassInterface());
}

}  // namespace mojo
