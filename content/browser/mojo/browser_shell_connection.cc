// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/browser_shell_connection.h"

#include "content/browser/mojo/constants.h"
#include "services/shell/public/interfaces/connector.mojom.h"

namespace content {

BrowserShellConnection::BrowserShellConnection(
    mojo::shell::mojom::ShellClientRequest request)
    : shell_connection_(new mojo::ShellConnection(this, std::move(request))) {}

BrowserShellConnection::~BrowserShellConnection() {}

mojo::Connector* BrowserShellConnection::GetConnector() {
  return shell_connection_->connector();
}

void BrowserShellConnection::AddEmbeddedApplication(
    const base::StringPiece& name,
    const EmbeddedApplicationRunner::FactoryCallback& callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  std::unique_ptr<EmbeddedApplicationRunner> app(
      new EmbeddedApplicationRunner(callback, task_runner));
  auto result = embedded_apps_.insert(
      std::make_pair(name.as_string(), std::move(app)));
  DCHECK(result.second);
}

bool BrowserShellConnection::AcceptConnection(mojo::Connection* connection) {
  std::string remote_app = connection->GetRemoteIdentity().name();
  if (remote_app == "mojo:shell") {
    // Only expose the SCF interface to the shell.
    connection->AddInterface<mojo::shell::mojom::ShellClientFactory>(this);
    return true;
  }

  // Allow connections from the root browser application.
  if (remote_app == kBrowserMojoApplicationName &&
      connection->GetRemoteIdentity().user_id() ==
          mojo::shell::mojom::kRootUserID)
    return true;

  // Reject all other connections to this application.
  return false;
}

void BrowserShellConnection::Create(
    mojo::Connection* connection,
    mojo::shell::mojom::ShellClientFactoryRequest request) {
  factory_bindings_.AddBinding(this, std::move(request));
}

void BrowserShellConnection::CreateShellClient(
    mojo::shell::mojom::ShellClientRequest request,
    const mojo::String& name) {
  auto it = embedded_apps_.find(name);
  if (it != embedded_apps_.end())
    it->second->BindShellClientRequest(std::move(request));
}

}  // namespace content
