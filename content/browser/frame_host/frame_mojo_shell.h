// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"

namespace content {

class RenderFrameHost;
class ServiceRegistryImpl;

// This provides the |mojo::shell::mojom::Shell| service interface to each
// frame's ServiceRegistry, giving frames the ability to connect to Mojo
// applications.
class FrameMojoShell : public mojo::shell::mojom::Connector {
 public:
  explicit FrameMojoShell(RenderFrameHost* frame_host);
  ~FrameMojoShell() override;

  void BindRequest(mojo::shell::mojom::ConnectorRequest request);

 private:
  // mojo::Connector:
  void Connect(
      mojo::shell::mojom::IdentityPtr target,
      mojo::shell::mojom::InterfaceProviderRequest services,
      mojo::shell::mojom::InterfaceProviderPtr exposed_services,
      const mojo::shell::mojom::Connector::ConnectCallback& callback) override;
  void Clone(mojo::shell::mojom::ConnectorRequest request) override;

  ServiceRegistryImpl* GetServiceRegistry();

  RenderFrameHost* frame_host_;
  mojo::BindingSet<mojo::shell::mojom::Connector> connectors_;

  // ServiceRegistry providing browser services to connected applications.
  scoped_ptr<ServiceRegistryImpl> service_registry_;
  mojo::BindingSet<mojo::shell::mojom::InterfaceProvider>
      service_provider_bindings_;

  DISALLOW_COPY_AND_ASSIGN(FrameMojoShell);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_
