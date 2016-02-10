// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

namespace content {

class RenderFrameHost;
class ServiceRegistryImpl;

// This provides the |mojo::shell::mojom::Shell| service interface to each
// frame's ServiceRegistry, giving frames the ability to connect to Mojo
// applications.
class FrameMojoShell : public mojo::shell::mojom::Shell {
 public:
  explicit FrameMojoShell(RenderFrameHost* frame_host);
  ~FrameMojoShell() override;

  void BindRequest(
      mojo::InterfaceRequest<mojo::shell::mojom::Shell> shell_request);

 private:
  // mojo::Shell:
  void ConnectToApplication(
      mojo::URLRequestPtr application_url,
      mojo::InterfaceRequest<mojo::InterfaceProvider> services,
      mojo::InterfaceProviderPtr exposed_services,
      mojo::shell::mojom::CapabilityFilterPtr filter,
      const ConnectToApplicationCallback& callback) override;
  void QuitApplication() override;

  ServiceRegistryImpl* GetServiceRegistry();

  RenderFrameHost* frame_host_;
  mojo::WeakBindingSet<mojo::shell::mojom::Shell> bindings_;

  // ServiceRegistry providing browser services to connected applications.
  scoped_ptr<ServiceRegistryImpl> service_registry_;
  mojo::WeakBindingSet<mojo::InterfaceProvider> service_provider_bindings_;

  DISALLOW_COPY_AND_ASSIGN(FrameMojoShell);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_
