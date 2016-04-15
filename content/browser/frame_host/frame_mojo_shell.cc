// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_mojo_shell.h"

#include <utility>

#include "build/build_config.h"
#include "content/browser/mojo/mojo_shell_context.h"
#include "content/common/mojo/service_registry_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "mojo/common/url_type_converters.h"

#if defined(OS_ANDROID) && defined(ENABLE_MOJO_CDM)
#include "content/browser/media/android/provision_fetcher_impl.h"
#endif

namespace content {

namespace {

void RegisterFrameMojoShellServices(ServiceRegistry* registry,
                                    RenderFrameHost* render_frame_host) {
#if defined(OS_ANDROID) && defined(ENABLE_MOJO_CDM)
  registry->AddService(
      base::Bind(&ProvisionFetcherImpl::Create, render_frame_host));
#endif
}

}  // namespace

FrameMojoShell::FrameMojoShell(RenderFrameHost* frame_host)
    : frame_host_(frame_host) {
}

FrameMojoShell::~FrameMojoShell() {
}

void FrameMojoShell::BindRequest(shell::mojom::ConnectorRequest request) {
  connectors_.AddBinding(this, std::move(request));
}

// TODO(xhwang): Currently no callers are exposing |exposed_services|. So we
// drop it and replace it with services we provide in the browser. In the
// future we may need to support both.
void FrameMojoShell::Connect(
    shell::mojom::IdentityPtr target,
    shell::mojom::InterfaceProviderRequest services,
    shell::mojom::InterfaceProviderPtr /* exposed_services */,
    shell::mojom::ClientProcessConnectionPtr client_process_connection,
    const shell::mojom::Connector::ConnectCallback& callback) {
  shell::mojom::InterfaceProviderPtr frame_services;
  service_provider_bindings_.AddBinding(GetServiceRegistry(),
                                        GetProxy(&frame_services));
  MojoShellContext::ConnectToApplication(
      shell::mojom::kRootUserID, target->name,
      frame_host_->GetSiteInstance()->GetSiteURL().spec(), std::move(services),
      std::move(frame_services), callback);
}

void FrameMojoShell::Clone(shell::mojom::ConnectorRequest request) {
  connectors_.AddBinding(this, std::move(request));
}

ServiceRegistryImpl* FrameMojoShell::GetServiceRegistry() {
  if (!service_registry_) {
    service_registry_.reset(new ServiceRegistryImpl());

    // TODO(rockot/xhwang): Currently all applications connected share the same
    // set of services registered in the |registry|. We may want to provide
    // different services for different apps for better isolation.
    RegisterFrameMojoShellServices(service_registry_.get(), frame_host_);
    GetContentClient()->browser()->RegisterFrameMojoShellServices(
        service_registry_.get(), frame_host_);
  }

  return service_registry_.get();
}

}  // namespace content
