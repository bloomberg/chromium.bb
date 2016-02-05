// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/converters/network/network_type_converters.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/environment/logging.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/lib/service_registry.h"

namespace mojo {

namespace {

void DefaultTerminationClosure() {
  if (base::MessageLoop::current() &&
      base::MessageLoop::current()->is_running())
    base::MessageLoop::current()->QuitWhenIdle();
}

}  // namespace

ApplicationImpl::ConnectParams::ConnectParams(const std::string& url)
    : ConnectParams(URLRequest::From(url)) {}
ApplicationImpl::ConnectParams::ConnectParams(URLRequestPtr request)
    : request_(std::move(request)), filter_(CapabilityFilter::New()) {
  filter_->filter.mark_non_null();
}
ApplicationImpl::ConnectParams::~ConnectParams() {}

ApplicationImpl::ApplicationImpl(ApplicationDelegate* delegate,
                                 InterfaceRequest<Application> request)
    : ApplicationImpl(delegate,
                      std::move(request),
                      base::Bind(&DefaultTerminationClosure)) {}

ApplicationImpl::ApplicationImpl(ApplicationDelegate* delegate,
                                 InterfaceRequest<Application> request,
                                 const Closure& termination_closure)
    : delegate_(delegate),
      binding_(this, std::move(request)),
      id_(Shell::kInvalidApplicationID),
      termination_closure_(termination_closure),
      app_lifetime_helper_(this),
      quit_requested_(false),
      weak_factory_(this) {}

ApplicationImpl::~ApplicationImpl() {
  app_lifetime_helper_.OnQuit();
}

scoped_ptr<ApplicationConnection> ApplicationImpl::ConnectToApplication(
    const std::string& url) {
  ConnectParams params(url);
  params.set_filter(CreatePermissiveCapabilityFilter());
  return ConnectToApplication(&params);
}

scoped_ptr<ApplicationConnection>
    ApplicationImpl::ConnectToApplication(ConnectParams* params) {
  if (!shell_)
    return nullptr;
  DCHECK(params);
  URLRequestPtr request = params->TakeRequest();
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
      application_url, application_url, Shell::kInvalidApplicationID,
      std::move(remote_services), std::move(local_request), allowed));
  shell_->ConnectToApplication(std::move(request),
                               std::move(remote_services_proxy),
                               std::move(local_services), params->TakeFilter(),
                               registry->GetConnectToApplicationCallback());
  return std::move(registry);
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

void ApplicationImpl::Initialize(ShellPtr shell,
                                 const mojo::String& url,
                                 uint32_t id) {
  shell_ = std::move(shell);
  shell_.set_connection_error_handler([this]() { OnConnectionError(); });
  url_ = url;
  id_ = id;
  delegate_->Initialize(this);
}

void ApplicationImpl::AcceptConnection(
    const String& requestor_url,
    uint32_t requestor_id,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    Array<String> allowed_interfaces,
    const String& url) {
  scoped_ptr<ApplicationConnection> registry(new internal::ServiceRegistry(
      url, requestor_url, requestor_id, std::move(exposed_services),
      std::move(services), allowed_interfaces.To<std::set<std::string>>()));
  if (!delegate_->ConfigureIncomingConnection(registry.get()))
    return;

  // If we were quitting because we thought there were no more services for this
  // app in use, then that has changed so cancel the quit request.
  if (quit_requested_)
    quit_requested_ = false;

  incoming_connections_.push_back(std::move(registry));
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

CapabilityFilterPtr CreatePermissiveCapabilityFilter() {
  CapabilityFilterPtr filter(CapabilityFilter::New());
  Array<String> all_interfaces;
  all_interfaces.push_back("*");
  filter->filter.insert("*", std::move(all_interfaces));
  return filter;
}

}  // namespace mojo
