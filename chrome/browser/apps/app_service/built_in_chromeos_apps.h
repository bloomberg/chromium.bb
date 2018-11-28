// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BUILT_IN_CHROMEOS_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BUILT_IN_CHROMEOS_APPS_H_

#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace apps {

// An app publisher (in the App Service sense) of built-in Chrome OS apps.
//
// See chrome/services/app_service/README.md.
class BuiltInChromeOsApps : public apps::mojom::Publisher {
 public:
  BuiltInChromeOsApps();
  ~BuiltInChromeOsApps() override;

  void Register(apps::mojom::AppServicePtr& app_service);

 private:
  // apps::mojom::Publisher overrides.
  void Connect(apps::mojom::SubscriberPtr subscriber,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                LoadIconCallback callback) override;

  mojo::Binding<apps::mojom::Publisher> binding_;

  DISALLOW_COPY_AND_ASSIGN(BuiltInChromeOsApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BUILT_IN_CHROMEOS_APPS_H_
