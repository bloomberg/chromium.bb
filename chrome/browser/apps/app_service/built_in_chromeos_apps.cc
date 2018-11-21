// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/built_in_chromeos_apps.h"

#include <utility>
#include <vector>

#include "mojo/public/cpp/bindings/interface_request.h"

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
  apps.push_back(std::move(app));

  subscriber->OnApps(std::move(apps));

  // Unlike other apps::mojom::Publisher implementations, we don't need to
  // retain the subscriber (e.g. add it to a
  // mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_) after this
  // function returns. The list of built-in Chrome OS apps is fixed for the
  // lifetime of the Chrome OS session. There won't be any further updates.
}

}  // namespace apps
