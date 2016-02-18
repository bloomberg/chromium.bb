// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_client_factory_connection.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/identity.h"

namespace mojo {
namespace shell {

ShellClientFactoryConnection::ShellClientFactoryConnection(
    ApplicationManager* manager,
    const Identity& source,
    const Identity& shell_client_factory,
    uint32_t id,
    const ClosedCallback& connection_closed_callback)
    : connection_closed_callback_(connection_closed_callback),
      identity_(shell_client_factory),
      connection_closed_(false),
      id_(id),
      ref_count_(0) {
  shell::mojom::InterfaceProviderPtr remote_interfaces;

  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->set_source(source);
  params->SetTarget(identity_);
  params->set_remote_interfaces(GetProxy(&remote_interfaces));
  manager->ConnectToApplication(std::move(params));

  MessagePipe pipe;
  shell_client_factory_.Bind(
      InterfacePtrInfo<mojom::ShellClientFactory>(std::move(pipe.handle0), 0u));
  remote_interfaces->GetInterface(mojom::ShellClientFactory::Name_,
                                  std::move(pipe.handle1));
  shell_client_factory_.set_connection_error_handler(
      [this]() { CloseConnection(); });
}

void ShellClientFactoryConnection::CreateShellClient(
    mojom::ShellClientRequest request,
    const GURL& url) {
  shell_client_factory_->CreateShellClient(
      std::move(request), url.spec(),
      base::Bind(&ShellClientFactoryConnection::ApplicationDestructed,
                 base::Unretained(this)));
  ref_count_++;
}

void ShellClientFactoryConnection::CloseConnection() {
  if (connection_closed_)
    return;
  connection_closed_ = true;
  connection_closed_callback_.Run(this);
  delete this;
}

ShellClientFactoryConnection::~ShellClientFactoryConnection() {
  // If this DCHECK fails then something has tried to delete this object without
  // calling CloseConnection.
  DCHECK(connection_closed_);
}

void ShellClientFactoryConnection::ApplicationDestructed() {
  if (!--ref_count_)
    CloseConnection();
}

}  // namespace shell
}  // namespace mojo
