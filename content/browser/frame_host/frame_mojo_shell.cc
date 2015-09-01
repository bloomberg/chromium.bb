// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_mojo_shell.h"

#include "content/browser/mojo/mojo_shell_context.h"
#include "content/common/mojo/service_registry_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"

namespace content {

FrameMojoShell::FrameMojoShell(RenderFrameHost* frame_host)
    : frame_host_(frame_host) {
}

FrameMojoShell::~FrameMojoShell() {
}

void FrameMojoShell::BindRequest(
    mojo::InterfaceRequest<mojo::Shell> shell_request) {
  bindings_.AddBinding(this, shell_request.Pass());
}

// TODO(xhwang): Currently no callers are exposing |exposed_services|. So we
// drop it and replace it with services we provide in the browser. In the
// future we may need to support both.
void FrameMojoShell::ConnectToApplication(
    mojo::URLRequestPtr application_url,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr /* exposed_services */,
    mojo::CapabilityFilterPtr filter,
    const ConnectToApplicationCallback& callback) {
  mojo::ServiceProviderPtr frame_services;
  service_provider_bindings_.AddBinding(GetServiceRegistry(),
                                        GetProxy(&frame_services));

  mojo::shell::CapabilityFilter capability_filter =
      mojo::shell::GetPermissiveCapabilityFilter();
  if (!filter.is_null())
    capability_filter = filter->filter.To<mojo::shell::CapabilityFilter>();
  MojoShellContext::ConnectToApplication(
      GURL(application_url->url), frame_host_->GetSiteInstance()->GetSiteURL(),
      services.Pass(), frame_services.Pass(), capability_filter, callback);
}

void FrameMojoShell::QuitApplication() {
}

ServiceRegistryImpl* FrameMojoShell::GetServiceRegistry() {
  if (!service_registry_) {
    service_registry_.reset(new ServiceRegistryImpl());

    GetContentClient()->browser()->RegisterFrameMojoShellServices(
        service_registry_.get(), frame_host_);
  }

  return service_registry_.get();
}

}  // namespace content
