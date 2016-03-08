// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/lib/connection_impl.h"
#include "mojo/shell/public/cpp/lib/connector_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace mojo {

////////////////////////////////////////////////////////////////////////////////
// ShellConnection, public:

ShellConnection::ShellConnection(
    mojo::ShellClient* client,
    InterfaceRequest<shell::mojom::ShellClient> request)
    : client_(client),
      binding_(this, std::move(request)),
      weak_factory_(this) {}

ShellConnection::~ShellConnection() {}

void ShellConnection::WaitForInitialize() {
  DCHECK(!connector_);
  binding_.WaitForIncomingMethodCall();
}

////////////////////////////////////////////////////////////////////////////////
// ShellConnection, shell::mojom::ShellClient implementation:

void ShellConnection::Initialize(shell::mojom::ConnectorPtr connector,
                                 shell::mojom::IdentityPtr identity,
                                 uint32_t id) {
  connector_.reset(new ConnectorImpl(std::move(connector)));
  binding_.set_connection_error_handler(
      base::Bind(&ShellConnection::OnConnectionError,
                 weak_factory_.GetWeakPtr()));
  client_->Initialize(connector_.get(), identity.To<Identity>(), id);
}

void ShellConnection::AcceptConnection(
    shell::mojom::IdentityPtr source,
    uint32_t source_id,
    shell::mojom::InterfaceProviderRequest local_interfaces,
    shell::mojom::InterfaceProviderPtr remote_interfaces,
    Array<String> allowed_interfaces,
    const String& name) {
  scoped_ptr<Connection> registry(new internal::ConnectionImpl(
      name, source.To<Identity>(), source_id, std::move(remote_interfaces),
      std::move(local_interfaces),
      allowed_interfaces.To<std::set<std::string>>(),
      Connection::State::CONNECTED));
  if (!client_->AcceptConnection(registry.get()))
    return;

  // TODO(beng): it appears we never prune this list. We should, when the
  //             connection's remote service provider pipe breaks.
  incoming_connections_.push_back(std::move(registry));
}

////////////////////////////////////////////////////////////////////////////////
// ShellConnection, private:

void ShellConnection::OnConnectionError() {
  // Note that the ShellClient doesn't technically have to quit now, it may live
  // on to service existing connections. All existing Connectors however are
  // invalid.
  client_->ShellConnectionLost();
  // We don't reset the connector as clients may have taken a raw pointer to it.
  // Connect() will return nullptr if they try to connect to anything.
}

}  // namespace mojo
