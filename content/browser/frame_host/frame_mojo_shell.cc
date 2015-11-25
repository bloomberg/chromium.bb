// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_mojo_shell.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/mojo/mojo_shell_context.h"
#include "content/common/mojo/service_registry_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/system/message_pipe.h"

#if defined(OS_ANDROID) && defined(ENABLE_MOJO_MEDIA)
#include "content/browser/media/android/provision_fetcher_impl.h"
#endif

namespace content {

namespace {

// This wraps a ServiceRegistryImpl in a mojo::ServiceProvider implementation
// so it can be bound to a mojo::ServiceProvider pipe.
class ServiceRegistryWrapper : public mojo::ServiceProvider {
 public:
  explicit ServiceRegistryWrapper(scoped_ptr<ServiceRegistryImpl> registry)
      : registry_(std::move(registry)) {
  }
  ~ServiceRegistryWrapper() override {}

 private:
  // mojo::ServiceProvider:
  void ConnectToService(const mojo::String& service_name,
                        mojo::ScopedMessagePipeHandle pipe) override {
    registry_->Connect(base::StringPiece(service_name), std::move(pipe));
  }

  const scoped_ptr<ServiceRegistryImpl> registry_;

  DISALLOW_COPY_AND_ASSIGN(ServiceRegistryWrapper);
};

void RegisterFrameMojoShellServices(ServiceRegistry* registry,
                                    RenderFrameHost* render_frame_host) {
#if defined(OS_ANDROID) && defined(ENABLE_MOJO_MEDIA)
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

void FrameMojoShell::BindRequest(
    mojo::InterfaceRequest<mojo::Shell> shell_request) {
  bindings_.AddBinding(this, std::move(shell_request));
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
  service_provider_bindings_.AddBinding(GetServiceProvider(),
                                        GetProxy(&frame_services));

  mojo::shell::CapabilityFilter capability_filter =
      mojo::shell::GetPermissiveCapabilityFilter();
  if (!filter.is_null())
    capability_filter = filter->filter.To<mojo::shell::CapabilityFilter>();
  MojoShellContext::ConnectToApplication(
      GURL(application_url->url), frame_host_->GetSiteInstance()->GetSiteURL(),
      std::move(services), std::move(frame_services), capability_filter,
      callback);
}

void FrameMojoShell::QuitApplication() {
}

mojo::ServiceProvider* FrameMojoShell::GetServiceProvider() {
  if (!service_provider_) {
    scoped_ptr<ServiceRegistryImpl> registry(new ServiceRegistryImpl());
    // TODO(rockot/xhwang): Currently all applications connected share the same
    // set of services registered in the |registry|. We may want to provide
    // different services for different apps for better isolation.
    RegisterFrameMojoShellServices(registry.get(), frame_host_);
    GetContentClient()->browser()->RegisterFrameMojoShellServices(
        registry.get(), frame_host_);
    service_provider_.reset(new ServiceRegistryWrapper(std::move(registry)));
  }
  return service_provider_.get();
}

}  // namespace content
