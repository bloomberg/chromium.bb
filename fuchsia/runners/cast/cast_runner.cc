// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <memory>
#include <string>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "fuchsia/runners/cast/cast_component.h"
#include "url/gurl.h"

CastRunner::CastRunner(
    base::fuchsia::ServiceDirectory* service_directory,
    chromium::web::ContextPtr context,
    chromium::cast::ApplicationConfigManagerPtr app_config_manager,
    base::OnceClosure on_idle_closure)
    : WebContentRunner(service_directory,
                       std::move(context),
                       std::move(on_idle_closure)),
      app_config_manager_(std::move(app_config_manager)) {
  app_config_manager_.set_error_handler([](zx_status_t status) {
    ZX_LOG(WARNING, status) << "ApplicationConfigManager disconnected";
  });
}

CastRunner::~CastRunner() = default;

void CastRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  // Verify that |package| specifies a Cast URI, and pull the app-Id from it.
  constexpr char kCastPresentationUrlScheme[] = "cast";
  constexpr char kCastSecurePresentationUrlScheme[] = "casts";

  GURL cast_url(package.resolved_url);
  if (!cast_url.is_valid() ||
      (!cast_url.SchemeIs(kCastPresentationUrlScheme) &&
       !cast_url.SchemeIs(kCastSecurePresentationUrlScheme)) ||
      cast_url.GetContent().empty()) {
    LOG(ERROR) << "Rejected invalid URL: " << cast_url;
    return;
  }

  // Fetch the Cast application configuration for the specified Id.
  const std::string cast_app_id(cast_url.GetContent());

  // TODO(https://crbug.com/933831): Look for ApplicationConfigManager in the
  // per-component incoming services. This works-around an issue with binding
  // to that service via the Runner's incoming services. Replace this with a
  // request for services from a Cast-specific Agent.
  auto startup_context =
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info));
  if (!app_config_manager_) {
    LOG(WARNING) << "Connect to ApplicationConfigManager from component /svc";
    startup_context->incoming_services()->ConnectToService(
        app_config_manager_.NewRequest());
  }

  app_config_manager_->GetConfig(
      cast_app_id,
      [this, startup_context = std::move(startup_context),
       controller_request = std::move(controller_request)](
          chromium::cast::ApplicationConfigPtr app_config) mutable {
        GetConfigCallback(std::move(startup_context),
                          std::move(controller_request), std::move(app_config));
      });
}

void CastRunner::GetConfigCallback(
    std::unique_ptr<base::fuchsia::StartupContext> startup_context,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request,
    chromium::cast::ApplicationConfigPtr app_config) {
  if (!app_config) {
    DLOG(WARNING) << "No ApplicationConfig was found.";
    return;
  }

  // If a config was returned then use it to launch a component.
  GURL cast_app_url(app_config->web_url);
  auto component = std::make_unique<CastComponent>(
      this, std::move(startup_context), std::move(controller_request));

  // Disable input for the Frame by default.
  component->frame()->SetEnableInput(false);

  component->LoadUrl(std::move(cast_app_url));
  RegisterComponent(std::move(component));
}
