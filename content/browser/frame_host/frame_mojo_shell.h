// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/shell/public/interfaces/connector.mojom.h"

namespace content {

class RenderFrameHost;
class ServiceRegistryImpl;

// This provides the |shell::mojom::Shell| service interface to each
// frame's ServiceRegistry, giving frames the ability to connect to Mojo
// applications.
class FrameMojoShell : public shell::mojom::Connector {
 public:
  explicit FrameMojoShell(RenderFrameHost* frame_host);
  ~FrameMojoShell() override;

  void BindRequest(shell::mojom::ConnectorRequest request);

 private:
  // shell::Connector:
  void Connect(
      shell::mojom::IdentityPtr target,
      shell::mojom::InterfaceProviderRequest services,
      shell::mojom::InterfaceProviderPtr exposed_services,
      shell::mojom::ClientProcessConnectionPtr client_process_connection,
      const shell::mojom::Connector::ConnectCallback& callback) override;
  void Clone(shell::mojom::ConnectorRequest request) override;

  ServiceRegistryImpl* GetServiceRegistry();

  RenderFrameHost* frame_host_;
  mojo::BindingSet<shell::mojom::Connector> connectors_;

  // ServiceRegistry providing browser services to connected applications.
  std::unique_ptr<ServiceRegistryImpl> service_registry_;
  mojo::BindingSet<shell::mojom::InterfaceProvider> service_provider_bindings_;

  DISALLOW_COPY_AND_ASSIGN(FrameMojoShell);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_
