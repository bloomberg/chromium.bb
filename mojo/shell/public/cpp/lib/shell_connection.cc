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

class AppRefCountImpl : public AppRefCount {
 public:
  AppRefCountImpl(ShellConnection* connection,
                  scoped_refptr<base::SingleThreadTaskRunner> app_task_runner)
      : connection_(connection),
        app_task_runner_(app_task_runner) {}
  ~AppRefCountImpl() override {
#ifndef NDEBUG
    // Ensure that this object is used on only one thread at a time, or else
    // there could be races where the object is being reset on one thread and
    // cloned on another.
    if (clone_task_runner_)
      DCHECK(clone_task_runner_->BelongsToCurrentThread());
#endif

    if (app_task_runner_->BelongsToCurrentThread()) {
      connection_->Release();
    } else {
      app_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ShellConnection::Release, base::Unretained(connection_)));
    }
  }

 private:
  // AppRefCount:
  scoped_ptr<AppRefCount> Clone() override {
    if (app_task_runner_->BelongsToCurrentThread()) {
      connection_->AddRef();
    } else {
      app_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ShellConnection::AddRef, base::Unretained(connection_)));
    }

#ifndef NDEBUG
    // Ensure that this object is used on only one thread at a time, or else
    // there could be races where the object is being reset on one thread and
    // cloned on another.
    if (clone_task_runner_) {
      DCHECK(clone_task_runner_->BelongsToCurrentThread());
    } else {
      clone_task_runner_ = base::MessageLoop::current()->task_runner();
    }
#endif

    return make_scoped_ptr(new AppRefCountImpl(connection_, app_task_runner_));
  }

  ShellConnection* connection_;
  scoped_refptr<base::SingleThreadTaskRunner> app_task_runner_;

#ifndef NDEBUG
  scoped_refptr<base::SingleThreadTaskRunner> clone_task_runner_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AppRefCountImpl);
};


ShellConnection::ConnectParams::ConnectParams(const std::string& url)
    : ConnectParams(URLRequest::From(url)) {}
ShellConnection::ConnectParams::ConnectParams(URLRequestPtr request)
    : request_(std::move(request)),
      filter_(shell::mojom::CapabilityFilter::New()) {
  filter_->filter.SetToEmpty();
}
ShellConnection::ConnectParams::~ConnectParams() {}

////////////////////////////////////////////////////////////////////////////////
// ShellConnection, public:

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
      quit_requested_(false),
      ref_count_(0),
      weak_factory_(this) {}

ShellConnection::~ShellConnection() {}

void ShellConnection::WaitForInitialize() {
  DCHECK(!shell_.is_bound());
  binding_.WaitForIncomingMethodCall();
}

////////////////////////////////////////////////////////////////////////////////
// ShellConnection, Shell implementation:

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
  std::string application_url = request->url.To<std::string>();
  // We allow all interfaces on outgoing connections since we are presumably in
  // a position to know who we're talking to.
  // TODO(beng): is this a valid assumption or do we need to figure some way to
  //             filter here too?
  std::set<std::string> allowed;
  allowed.insert("*");
  shell::mojom::InterfaceProviderPtr local_interfaces;
  shell::mojom::InterfaceProviderRequest local_request =
      GetProxy(&local_interfaces);
  shell::mojom::InterfaceProviderPtr remote_interfaces;
  shell::mojom::InterfaceProviderRequest remote_request =
      GetProxy(&remote_interfaces);
  scoped_ptr<internal::ConnectionImpl> registry(new internal::ConnectionImpl(
      application_url, application_url,
      shell::mojom::Shell::kInvalidApplicationID, std::move(remote_interfaces),
      std::move(local_request), allowed));
  shell_->ConnectToApplication(std::move(request),
                               std::move(remote_request),
                               std::move(local_interfaces),
                               params->TakeFilter(),
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
  AddRef();
  return make_scoped_ptr(
      new AppRefCountImpl(this, base::MessageLoop::current()->task_runner()));
}

////////////////////////////////////////////////////////////////////////////////
// ShellConnection, shell::mojom::ShellClient implementation:

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
    shell::mojom::InterfaceProviderRequest local_interfaces,
    shell::mojom::InterfaceProviderPtr remote_interfaces,
    Array<String> allowed_interfaces,
    const String& url) {
  scoped_ptr<Connection> registry(new internal::ConnectionImpl(
      url, requestor_url, requestor_id, std::move(remote_interfaces),
      std::move(local_interfaces),
      allowed_interfaces.To<std::set<std::string>>()));
  if (!client_->AcceptConnection(registry.get()))
    return;

  // If we were quitting because we thought there were no more interfaces for
  // this app in use, then that has changed so cancel the quit request.
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

////////////////////////////////////////////////////////////////////////////////
// ShellConnection, private:

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

void ShellConnection::AddRef() {
  ++ref_count_;
}

void ShellConnection::Release() {
  if (!--ref_count_)
    Quit();
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
