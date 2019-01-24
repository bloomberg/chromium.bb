// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/common/web_component.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fit/function.h>
#include <utility>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/logging.h"
#include "fuchsia/runners/common/web_content_runner.h"

WebComponent::~WebComponent() {
  // Send process termination details to the client.
  controller_binding_.events().OnTerminated(termination_exit_code_,
                                            termination_reason_);
}

// static
std::unique_ptr<WebComponent> WebComponent::ForUrlRequest(
    WebContentRunner* runner,
    const GURL& url,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  DCHECK(url.is_valid());
  std::unique_ptr<WebComponent> component(new WebComponent(
      runner, std::move(startup_info), std::move(controller_request)));
  chromium::web::NavigationControllerPtr navigation_controller;
  component->frame()->GetNavigationController(
      navigation_controller.NewRequest());

  // Set the page activation flag on the initial load, so that features like
  // autoplay work as expected when a WebComponent first loads the specified
  // content.
  auto params = std::make_unique<chromium::web::LoadUrlParams>();
  params->user_activated = true;

  navigation_controller->LoadUrl(url.spec(), std::move(params));

  return component;
}

WebComponent::WebComponent(
    WebContentRunner* runner,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request)
    : runner_(runner), controller_binding_(this) {
  DCHECK(runner);

  // If the ComponentController request is valid then bind it, and configure it
  // to destroy this component on error.
  if (controller_request.is_valid()) {
    controller_binding_.Bind(std::move(controller_request));
    controller_binding_.set_error_handler([this](zx_status_t status) {
      // Signal graceful process termination.
      DestroyComponent(0, fuchsia::sys::TerminationReason::EXITED);
    });
  }

  // Create the underlying Frame and get its NavigationController.
  runner_->context()->CreateFrame(frame_.NewRequest());

  // Create a ServiceDirectory for this component, and publish a ViewProvider
  // into it, for the caller to use to create a View for this component.
  // Note that we must publish ViewProvider before returning control to the
  // message-loop, to ensure that it is available before the ServiceDirectory
  // starts processing requests.
  service_directory_ = std::make_unique<base::fuchsia::ServiceDirectory>(
      std::move(startup_info.launch_info.directory_request));
  view_provider_binding_ = std::make_unique<
      base::fuchsia::ScopedServiceBinding<fuchsia::ui::app::ViewProvider>>(
      service_directory_.get(), this);
  legacy_view_provider_binding_ = std::make_unique<
      base::fuchsia::ScopedServiceBinding<fuchsia::ui::viewsv1::ViewProvider>>(
      service_directory_.get(), this);
}

void WebComponent::Kill() {
  // Signal abnormal process termination.
  DestroyComponent(1, fuchsia::sys::TerminationReason::RUNNER_TERMINATED);
}

void WebComponent::Detach() {
  controller_binding_.set_error_handler(nullptr);
}

void WebComponent::CreateView(
    zx::eventpair view_token,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
    fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services) {
  DCHECK(frame_);
  DCHECK(!view_is_bound_);

  frame_->CreateView2(std::move(view_token), std::move(incoming_services),
                      std::move(outgoing_services));

  view_is_bound_ = true;
}

void WebComponent::CreateView(
    fidl::InterfaceRequest<fuchsia::ui::viewsv1token::ViewOwner> view_owner,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> services) {
  // Cast the ViewOwner request to view_token. This is temporary hack for
  // ViewsV2 transition. This version of CreateView() will be removed in the
  // future.
  CreateView(zx::eventpair(view_owner.TakeChannel().release()),
             std::move(services), nullptr);
}

void WebComponent::DestroyComponent(int termination_exit_code,
                                    fuchsia::sys::TerminationReason reason) {
  termination_reason_ = reason;
  termination_exit_code_ = termination_exit_code;
  runner_->DestroyComponent(this);
}
