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
#include "mojo/shell/public/cpp/lib/connection_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace mojo {

namespace {

void DefaultTerminationClosure() {
  if (base::MessageLoop::current() &&
      base::MessageLoop::current()->is_running())
    base::MessageLoop::current()->QuitWhenIdle();
}

}  // namespace

ShellConnection::ConnectParams::ConnectParams(const std::string& url)
    : ConnectParams(URLRequest::From(url)) {}
ShellConnection::ConnectParams::ConnectParams(URLRequestPtr request)
    : request_(std::move(request)),
      filter_(shell::mojom::CapabilityFilter::New()) {
  filter_->filter.mark_non_null();
}
ShellConnection::ConnectParams::~ConnectParams() {}

ShellConnection::ShellConnection(
    mojo::ShellClient* client,
    InterfaceRequest<shell::mojom::ShellClient> request)
    : ShellConnection(client,
                      std::move(request),
                      base::Bind(&DefaultTerminationClosure)) {}

ShellConnection::ShellConnection(
    mojo::ShellClient* client,
    InterfaceRequest<shell::mojom::ShellClient> request,
    const Closure& termination_closure)
    : client_(client),
      binding_(this, std::move(request)),
      termination_closure_(termination_closure),
      app_lifetime_helper_(this),
      quit_requested_(false),
      weak_factory_(this) {}

ShellConnection::~ShellConnection() {
  app_lifetime_helper_.OnQuit();
}

void ShellConnection::WaitForInitialize() {
  DCHECK(!shell_.is_bound());
  binding_.WaitForIncomingMethodCall();
}

scoped_ptr<Connection> ShellConnection::Connect(const std::string& url) {
  ConnectParams params(url);
  params.set_filter(CreatePermissiveCapabilityFilter());
  return Connect(&params);
}

scoped_ptr<Connection> ShellConnection::Connect(ConnectParams* params) {
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
  scoped_ptr<internal::ConnectionImpl> registry(new internal::ConnectionImpl(
      application_url, application_url,
      shell::mojom::Shell::kInvalidApplicationID, std::move(remote_services),
      std::move(local_request), allowed));
  shell_->ConnectToApplication(std::move(request),
                               std::move(remote_services_proxy),
                               std::move(local_services), params->TakeFilter(),
                               registry->GetConnectToApplicationCallback());
  return std::move(registry);
}

void ShellConnection::Quit() {
  // We can't quit immediately, since there could be in-flight requests from the
  // shell. So check with it first.
  if (shell_) {
    quit_requested_ = true;
    shell_->QuitApplication();
  } else {
    QuitNow();
  }
}

scoped_ptr<AppRefCount> ShellConnection::CreateAppRefCount() {
  return app_lifetime_helper_.CreateAppRefCount();
}

void ShellConnection::Initialize(shell::mojom::ShellPtr shell,
                                 const mojo::String& url,
                                 uint32_t id) {
  shell_ = std::move(shell);
  shell_.set_connection_error_handler([this]() { OnConnectionError(); });
  client_->Initialize(this, url, id);
}

void ShellConnection::AcceptConnection(
    const String& requestor_url,
    uint32_t requestor_id,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    Array<String> allowed_interfaces,
    const String& url) {
  scoped_ptr<Connection> registry(new internal::ConnectionImpl(
      url, requestor_url, requestor_id, std::move(exposed_services),
      std::move(services), allowed_interfaces.To<std::set<std::string>>()));
  if (!client_->AcceptConnection(registry.get()))
    return;

  // If we were quitting because we thought there were no more services for this
  // app in use, then that has changed so cancel the quit request.
  if (quit_requested_)
    quit_requested_ = false;

  incoming_connections_.push_back(std::move(registry));
}

void ShellConnection::OnQuitRequested(const Callback<void(bool)>& callback) {
  // If by the time we got the reply from the shell, more requests had come in
  // then we don't want to quit the app anymore so we return false. Otherwise
  // |quit_requested_| is true so we tell the shell to proceed with the quit.
  callback.Run(quit_requested_);
  if (quit_requested_)
    QuitNow();
}

void ShellConnection::OnConnectionError() {
  base::WeakPtr<ShellConnection> ptr(weak_factory_.GetWeakPtr());

  // We give the client notice first, since it might want to do something on
  // shell connection errors other than immediate termination of the run
  // loop. The application might want to continue servicing connections other
  // than the one to the shell.
  bool quit_now = client_->ShellConnectionLost();
  if (quit_now)
    QuitNow();
  if (!ptr)
    return;
  shell_ = nullptr;
}

void ShellConnection::QuitNow() {
  client_->Quit();
  termination_closure_.Run();
}

void ShellConnection::UnbindConnections(
    InterfaceRequest<shell::mojom::ShellClient>* request,
    shell::mojom::ShellPtr* shell) {
  *request = binding_.Unbind();
  shell->Bind(shell_.PassInterface());
}

shell::mojom::CapabilityFilterPtr CreatePermissiveCapabilityFilter() {
  shell::mojom::CapabilityFilterPtr filter(
      shell::mojom::CapabilityFilter::New());
  Array<String> all_interfaces;
  all_interfaces.push_back("*");
  filter->filter.insert("*", std::move(all_interfaces));
  return filter;
}

}  // namespace mojo
