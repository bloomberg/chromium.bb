// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {

class RenderFrameHost;
class ServiceRegistryImpl;

// This provides the |mojo::Shell| service interface to each frame's
// ServiceRegistry, giving frames the ability to connect to Mojo applications.
class FrameMojoShell : public mojo::Shell {
 public:
  explicit FrameMojoShell(RenderFrameHost* frame_host);
  ~FrameMojoShell() override;

  void BindRequest(mojo::InterfaceRequest<mojo::Shell> shell_request);

 private:
  // mojo::Shell:
  void ConnectToApplication(
      mojo::URLRequestPtr application_url,
      mojo::InterfaceRequest<mojo::ServiceProvider> services,
      mojo::ServiceProviderPtr exposed_services,
      mojo::CapabilityFilterPtr filter,
      const ConnectToApplicationCallback& callback) override;
  void QuitApplication() override;

  mojo::ServiceProvider* GetServiceProvider();

  RenderFrameHost* frame_host_;
  mojo::WeakBindingSet<mojo::Shell> bindings_;
  scoped_ptr<mojo::ServiceProvider> service_provider_;

  mojo::WeakBindingSet<mojo::ServiceProvider> service_provider_bindings_;

  DISALLOW_COPY_AND_ASSIGN(FrameMojoShell);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_MOJO_SHELL_H_
