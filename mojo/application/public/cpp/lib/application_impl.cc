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
    base::MessageLoop::current()->Quit();
}

}  // namespace

// TODO(beng): upstream this into mojo repo, array.h
template <typename E, typename T>
struct TypeConverter<std::set<E>, Array<T>> {
  static std::set<E> Convert(const Array<T>& input) {
    std::set<E> result;
    if (!input.is_null()) {
      for (size_t i = 0; i < input.size(); ++i)
        result.insert(TypeConverter<E, T>::Convert(input[i]));
    }
    return result;
  }
};

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
      in_destructor_(false),
      weak_factory_(this) {}

ApplicationImpl::~ApplicationImpl() {
  DCHECK(!in_destructor_);
  in_destructor_ = true;
  ClearConnections();
  app_lifetime_helper_.OnQuit();
}

ApplicationConnection* ApplicationImpl::ConnectToApplication(
    mojo::URLRequestPtr request,
    CapabilityFilterPtr filter) {
  if (!shell_)
    return nullptr;
  ServiceProviderPtr local_services;
  InterfaceRequest<ServiceProvider> local_request = GetProxy(&local_services);
  ServiceProviderPtr remote_services;
  std::string application_url = request->url.To<std::string>();
  shell_->ConnectToApplication(request.Pass(), GetProxy(&remote_services),
                               local_services.Pass(), filter.Pass());
  // We allow all interfaces on outgoing connections since we are presumably in
  // a position to know who we're talking to.
  // TODO(beng): is this a valid assumption or do we need to figure some way to
  //             filter here too?
  std::set<std::string> allowed;
  allowed.insert("*");
  internal::ServiceRegistry* registry = new internal::ServiceRegistry(
      this, application_url, application_url, remote_services.Pass(),
      local_request.Pass(), allowed);
  if (!delegate_->ConfigureOutgoingConnection(registry)) {
    registry->CloseConnection();
    return nullptr;
  }
  outgoing_service_registries_.push_back(registry);
  return registry;
}

void ApplicationImpl::CloseConnection(ApplicationConnection* connection) {
  if (!in_destructor_)
    delegate_->OnWillCloseConnection(connection);
  auto outgoing_it = std::find(outgoing_service_registries_.begin(),
                               outgoing_service_registries_.end(),
                               connection);
  if (outgoing_it != outgoing_service_registries_.end()) {
    outgoing_service_registries_.erase(outgoing_it);
    return;
  }
  auto incoming_it = std::find(incoming_service_registries_.begin(),
                               incoming_service_registries_.end(),
                               connection);
 if (incoming_it != incoming_service_registries_.end())
    incoming_service_registries_.erase(incoming_it);
}

void ApplicationImpl::Initialize(ShellPtr shell, const mojo::String& url) {
  shell_ = shell.Pass();
  shell_.set_connection_error_handler([this]() { OnConnectionError(); });
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

void ApplicationImpl::AcceptConnection(
    const String& requestor_url,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    Array<String> allowed_interfaces,
    const String& url) {
  internal::ServiceRegistry* registry = new internal::ServiceRegistry(
      this, url, requestor_url, exposed_services.Pass(), services.Pass(),
      allowed_interfaces.To<std::set<std::string>>());
  if (!delegate_->ConfigureIncomingConnection(registry)) {
    registry->CloseConnection();
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

void ApplicationImpl::ClearConnections() {
  // Copy the ServiceRegistryLists because they will be mutated by
  // ApplicationConnection::CloseConnection.
  ServiceRegistryList incoming_service_registries(incoming_service_registries_);
  for (internal::ServiceRegistry* registry : incoming_service_registries)
    registry->CloseConnection();
  DCHECK(incoming_service_registries_.empty());

  ServiceRegistryList outgoing_service_registries(outgoing_service_registries_);
  for (internal::ServiceRegistry* registry : outgoing_service_registries)
    registry->CloseConnection();
  DCHECK(outgoing_service_registries_.empty());
}

void ApplicationImpl::QuitNow() {
  delegate_->Quit();
  termination_closure_.Run();
}

}  // namespace mojo
