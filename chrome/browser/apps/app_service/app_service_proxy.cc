// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy.h"

#include <utility>

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/public/mojom/constants.mojom.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"

namespace apps {

// static
AppServiceProxy* AppServiceProxy::Get(Profile* profile) {
  return AppServiceProxyFactory::GetForProfile(profile);
}

AppServiceProxy::AppServiceProxy(Profile* profile) {
  content::BrowserContext::GetConnectorFor(profile)->BindInterface(
      apps::mojom::kServiceName, mojo::MakeRequest(&app_service_));

  // The AppServiceProxy is a subscriber: something that wants to be able to
  // list all known apps.
  apps::mojom::SubscriberPtr subscriber;
  bindings_.AddBinding(this, mojo::MakeRequest(&subscriber));
  app_service_->RegisterSubscriber(std::move(subscriber), nullptr);

#if defined(OS_CHROMEOS)
  // The AppServiceProxy is also a publisher, of built-in apps. That
  // responsibility isn't intrinsically part of the AppServiceProxy, but doing
  // that here is as good a place as any.
  built_in_chrome_os_apps_.Initialize(app_service_, profile);
#endif  // OS_CHROMEOS
}

AppServiceProxy::~AppServiceProxy() = default;

AppRegistryCache& AppServiceProxy::Cache() {
  return cache_;
}

void AppServiceProxy::LoadIcon(
    const std::string& app_id,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    apps::mojom::Publisher::LoadIconCallback callback) {
  bool found = false;
  cache_.ForOneApp(app_id, [this, &icon_compression, &size_hint_in_dip,
                            &callback, &found](const apps::AppUpdate& update) {
    apps::mojom::IconKeyPtr icon_key = update.IconKey();
    if (icon_key.is_null()) {
      return;
    }
    found = true;
    app_service_->LoadIcon(update.AppType(), update.AppId(),
                           std::move(icon_key), icon_compression,
                           size_hint_in_dip, std::move(callback));
  });

  if (!found) {
    // On failure, we still run the callback, with the zero IconValue.
    std::move(callback).Run(apps::mojom::IconValue::New());
  }
}

void AppServiceProxy::OnApps(std::vector<apps::mojom::AppPtr> deltas) {
  cache_.OnApps(std::move(deltas));
}

void AppServiceProxy::Clone(apps::mojom::SubscriberRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace apps
