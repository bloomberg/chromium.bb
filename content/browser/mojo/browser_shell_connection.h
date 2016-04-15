// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_BROWSER_SHELL_CONNECTION_H_
#define CONTENT_BROWSER_MOJO_BROWSER_SHELL_CONNECTION_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "content/common/mojo/embedded_application_runner.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"
#include "services/shell/public/interfaces/shell_client_factory.mojom.h"

namespace content {

// A connection from the browser process to the Mojo shell. There may be
// multiple connections in a single browser process. Each connection may have
// its own identity, e.g., a connection with unique user ID per BrowserContext.
class BrowserShellConnection
    : public shell::ShellClient,
      public shell::InterfaceFactory<shell::mojom::ShellClientFactory>,
      public shell::mojom::ShellClientFactory {
 public:
  using ShellClientRequestHandler =
      base::Callback<void(shell::mojom::ShellClientRequest)>;

  // Constructs a connection which does not own a ShellClient pipe. This
  // connection must be manually fed new incoming connections via its
  // shell::ShellClient interface.
  BrowserShellConnection();

  // Constructs a connection associated with its own ShellClient pipe.
  explicit BrowserShellConnection(shell::mojom::ShellClientRequest request);

  ~BrowserShellConnection() override;

  shell::Connector* GetConnector();

  // Adds an embedded application to this connection's ShellClientFactory.
  // |callback| will be used to create a new instance of the application on
  // |task_runner|'s thread if no instance is running when an incoming
  // connection is made to |name|. If |task_runner| is null, the calling thread
  // will be used to run the application.
  void AddEmbeddedApplication(
      const base::StringPiece& name,
      const EmbeddedApplicationRunner::FactoryCallback& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Adds a generic ShellClientRequestHandler for a given application name. This
  // will be used to satisfy any incoming calls to CreateShellClient() which
  // reference the given name.
  //
  // For in-process applications, it is preferrable to use
  // |AddEmbeddedApplication()| as defined above.
  void AddShellClientRequestHandler(const base::StringPiece& name,
                                    const ShellClientRequestHandler& handler);

 private:
  // shell::ShellClient:
  bool AcceptConnection(shell::Connection* connection) override;

  // shell::InterfaceFactory<shell::mojom::ShellClientFactory>:
  void Create(shell::Connection* connection,
              shell::mojom::ShellClientFactoryRequest request) override;

  // shell::mojom::ShellClientFactory:
  void CreateShellClient(shell::mojom::ShellClientRequest request,
                         const mojo::String& name) override;

  std::unique_ptr<shell::ShellConnection> shell_connection_;
  mojo::BindingSet<shell::mojom::ShellClientFactory> factory_bindings_;
  std::unordered_map<std::string, std::unique_ptr<EmbeddedApplicationRunner>>
      embedded_apps_;
  std::unordered_map<std::string, ShellClientRequestHandler> request_handlers_;

  DISALLOW_COPY_AND_ASSIGN(BrowserShellConnection);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_BROWSER_SHELL_CONNECTION_H_
