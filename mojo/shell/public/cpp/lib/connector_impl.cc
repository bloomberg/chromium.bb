// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/lib/connector_impl.h"

#include "mojo/shell/public/cpp/lib/connection_impl.h"

namespace mojo {

Connector::ConnectParams::ConnectParams(const std::string& name)
    : name_(name),
      user_id_(shell::mojom::kInheritUserID) {
}
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
  std::string application_name = params->name();
  // We allow all interfaces on outgoing connections since we are presumably in
  // a position to know who we're talking to.
  // TODO(beng): We should filter outgoing interfaces also. The shell must pass
  //             the manifest CapabilityFilter to the ShellConnection via
  //             Initialize(), it can be used here.
  std::set<std::string> allowed;
  allowed.insert("*");
  shell::mojom::InterfaceProviderPtr local_interfaces;
  shell::mojom::InterfaceProviderRequest local_request =
      GetProxy(&local_interfaces);
  shell::mojom::InterfaceProviderPtr remote_interfaces;
  shell::mojom::InterfaceProviderRequest remote_request =
      GetProxy(&remote_interfaces);
  scoped_ptr<internal::ConnectionImpl> registry(new internal::ConnectionImpl(
      application_name, application_name,
      shell::mojom::Connector::kInvalidApplicationID, params->user_id(),
      std::move(remote_interfaces), std::move(local_request), allowed));
  connector_->Connect(application_name,
                      params->user_id(),
                      std::move(remote_request),
                      std::move(local_interfaces),
                      registry->GetConnectCallback());
  return std::move(registry);
}

scoped_ptr<Connector> ConnectorImpl::Clone() {
  shell::mojom::ConnectorPtr connector;
  connector_->Clone(GetProxy(&connector));
  return make_scoped_ptr(
      new ConnectorImpl(connector.PassInterface()));
}

}  // namespace mojo
