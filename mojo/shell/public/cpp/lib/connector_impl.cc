// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/lib/connector_impl.h"

#include "mojo/shell/public/cpp/identity.h"
#include "mojo/shell/public/cpp/lib/connection_impl.h"

namespace mojo {

Connector::ConnectParams::ConnectParams(const Identity& target)
    : target_(target) {}
Connector::ConnectParams::ConnectParams(const std::string& name)
    : target_(name, shell::mojom::kInheritUserID) {}
Connector::ConnectParams::~ConnectParams() {}

ConnectorImpl::ConnectorImpl(shell::mojom::ConnectorPtrInfo unbound_state)
    : unbound_state_(std::move(unbound_state)) {}
ConnectorImpl::ConnectorImpl(shell::mojom::ConnectorPtr connector)
    : connector_(std::move(connector)) {
  thread_checker_.reset(new base::ThreadChecker);
}
ConnectorImpl::~ConnectorImpl() {}

scoped_ptr<Connection> ConnectorImpl::Connect(const std::string& name) {
  ConnectParams params(name);
  return Connect(&params);
}

scoped_ptr<Connection> ConnectorImpl::Connect(ConnectParams* params) {
  // Bind this object to the current thread the first time it is used to
  // connect.
  if (!connector_.is_bound()) {
    if (!unbound_state_.is_valid()) {
      // It's possible to get here when the link to the shell has been severed
      // (and so the connector pipe has been closed) but the app has chosen not
      // to quit.
      return nullptr;
    }
    connector_.Bind(std::move(unbound_state_));
    thread_checker_.reset(new base::ThreadChecker);
  }
  DCHECK(thread_checker_->CalledOnValidThread());

  DCHECK(params);
  // We allow all interfaces on outgoing connections since we are presumably in
  // a position to know who we're talking to.
  CapabilityRequest request;
  request.interfaces.insert("*");
  shell::mojom::InterfaceProviderPtr local_interfaces;
  shell::mojom::InterfaceProviderRequest local_request =
      GetProxy(&local_interfaces);
  shell::mojom::InterfaceProviderPtr remote_interfaces;
  shell::mojom::InterfaceProviderRequest remote_request =
      GetProxy(&remote_interfaces);
  scoped_ptr<internal::ConnectionImpl> registry(new internal::ConnectionImpl(
      params->target().name(), params->target(),
      shell::mojom::kInvalidInstanceID, std::move(remote_interfaces),
      std::move(local_request), request, Connection::State::PENDING));

  shell::mojom::ShellClientPtr shell_client;
  shell::mojom::PIDReceiverRequest pid_receiver_request;
  params->TakeClientProcessConnection(&shell_client, &pid_receiver_request);
  shell::mojom::ClientProcessConnectionPtr client_process_connection;
  if (shell_client.is_bound() && pid_receiver_request.is_pending()) {
    client_process_connection = shell::mojom::ClientProcessConnection::New();
    client_process_connection->shell_client =
        shell_client.PassInterface().PassHandle();
    client_process_connection->pid_receiver_request =
        pid_receiver_request.PassMessagePipe();
  } else if (shell_client.is_bound() || pid_receiver_request.is_pending()) {
    NOTREACHED() << "If one of shell_client or pid_receiver_request is valid, "
                 << "both must be valid.";
    return std::move(registry);
  }
  connector_->Connect(
      shell::mojom::Identity::From(params->target()),
      std::move(remote_request), std::move(local_interfaces),
      std::move(client_process_connection), registry->GetConnectCallback());
  return std::move(registry);
}

scoped_ptr<Connector> ConnectorImpl::Clone() {
  shell::mojom::ConnectorPtr connector;
  connector_->Clone(GetProxy(&connector));
  return make_scoped_ptr(
      new ConnectorImpl(connector.PassInterface()));
}

}  // namespace mojo
