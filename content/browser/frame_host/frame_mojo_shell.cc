// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_mojo_shell.h"

#include "content/browser/mojo/mojo_shell_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"

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

void FrameMojoShell::ConnectToApplication(
    mojo::URLRequestPtr application_url,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  MojoShellContext::ConnectToApplication(
      GURL(application_url->url), frame_host_->GetSiteInstance()->GetSiteURL(),
      services.Pass());
}

void FrameMojoShell::QuitApplication() {
}

}  // namespace content
