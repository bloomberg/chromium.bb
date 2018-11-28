// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/built_in_chromeos_apps.h"

#include <utility>
#include <vector>

#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"

// TODO: delete this when we no longer refer to IDR_APP_DEFAULT_ICON below.
#include "extensions/grit/extensions_browser_resources.h"

namespace apps {

BuiltInChromeOsApps::BuiltInChromeOsApps() : binding_(this) {}

BuiltInChromeOsApps::~BuiltInChromeOsApps() = default;

void BuiltInChromeOsApps::Register(apps::mojom::AppServicePtr& app_service) {
  apps::mojom::PublisherPtr publisher;
  binding_.Bind(mojo::MakeRequest(&publisher));
  app_service->RegisterPublisher(std::move(publisher),
                                 apps::mojom::AppType::kBuiltIn);
}

void BuiltInChromeOsApps::Connect(apps::mojom::SubscriberPtr subscriber,
                                  apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;

  // TODO: replace the placeholder app with real ones.
  //
  // See also "TODO: process real apps..." in app_service_proxy.h.
  auto app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kBuiltIn;
  app->app_id = "placeholder";
  app->readiness = apps::mojom::Readiness::kReady;
  app->name = "Holder of the Place";
  app->icon_key = apps::mojom::IconKey::New();
  app->icon_key->icon_type = apps::mojom::IconType::kResource;
  app->icon_key->u_key = IDR_APP_DEFAULT_ICON;
  app->show_in_launcher = apps::mojom::OptionalBool::kTrue;
  apps.push_back(std::move(app));

  subscriber->OnApps(std::move(apps));

  // Unlike other apps::mojom::Publisher implementations, we don't need to
  // retain the subscriber (e.g. add it to a
  // mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_) after this
  // function returns. The list of built-in Chrome OS apps is fixed for the
  // lifetime of the Chrome OS session. There won't be any further updates.
}

void BuiltInChromeOsApps::LoadIcon(
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    LoadIconCallback callback) {
  if (!icon_key.is_null() &&
      (icon_key->icon_type == apps::mojom::IconType::kResource) &&
      (icon_key->u_key != 0) && (icon_key->u_key <= INT_MAX)) {
    int resource_id = static_cast<int>(icon_key->u_key);
    LoadIconFromResource(icon_compression, size_hint_in_dip,
                         std::move(callback), resource_id);
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

}  // namespace apps
