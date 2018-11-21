// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/web_application_info.h"

namespace web_app {

WebAppInstallFinalizer::WebAppInstallFinalizer(WebAppRegistrar* registrar)
    : registrar_(registrar) {}

WebAppInstallFinalizer::~WebAppInstallFinalizer() = default;

void WebAppInstallFinalizer::FinalizeInstall(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallFinalizedCallback callback) {
  const AppId app_id = GenerateAppIdFromURL(web_app_info->app_url);
  auto web_app = std::make_unique<WebApp>(app_id);

  web_app->SetName(base::UTF16ToUTF8(web_app_info->title));
  web_app->SetDescription(base::UTF16ToUTF8(web_app_info->description));
  web_app->SetLaunchUrl(web_app_info->app_url);
  web_app->SetScope(web_app_info->scope);
  web_app->SetThemeColor(web_app_info->theme_color);

  // TODO(loyso): Add web_app_info->icons into web_app. Save them on disk.

  registrar_->RegisterApp(std::move(web_app));

  std::move(callback).Run(app_id, InstallResultCode::kSuccess);
}

}  // namespace web_app
