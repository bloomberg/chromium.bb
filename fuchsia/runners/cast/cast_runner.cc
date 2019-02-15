// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
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
      app_config_manager_(std::move(app_config_manager)) {}

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
  app_config_manager_->GetConfig(
      cast_app_id,
      [this,
       startup_context = std::make_unique<base::fuchsia::StartupContext>(
           std::move(startup_info)),
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

    // For test purposes, we need to call RegisterComponent even if there is no
    // URL to launch.
    // TODO: Replace this hack, e.g. with an test-specific callback.
    RegisterComponent(std::unique_ptr<WebComponent>(nullptr));
    return;
  }

  // If a config was returned then use it to launch a component.
  GURL cast_app_url(app_config->web_url);
  auto component = std::make_unique<CastComponent>(
      this, std::move(startup_context), std::move(controller_request));
  component->LoadUrl(std::move(cast_app_url));
  RegisterComponent(std::move(component));
}
