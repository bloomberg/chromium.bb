// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"

namespace web_app {

namespace {

// Use tricky function adapters here to connect old API with new unique_ptr
// based API. TODO(loyso): Erase these type adapters. crbug.com/915043.
using AcceptanceCallback = InstallManager::WebAppInstallationAcceptanceCallback;

void BookmarkAppAcceptanceCallback(
    AcceptanceCallback web_app_acceptance_callback,
    bool user_accepted,
    const WebApplicationInfo& web_app_info) {
  std::move(web_app_acceptance_callback)
      .Run(user_accepted, std::make_unique<WebApplicationInfo>(web_app_info));
}

void WebAppInstallDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    AcceptanceCallback web_app_acceptance_callback) {
  if (base::FeatureList::IsEnabled(::features::kDesktopPWAWindowing) &&
      for_installable_site == ForInstallableSite::kYes) {
    web_app_info->open_as_window = true;
    chrome::ShowPWAInstallDialog(
        initiator_web_contents, *web_app_info,
        base::BindOnce(BookmarkAppAcceptanceCallback,
                       std::move(web_app_acceptance_callback)));
  } else {
    chrome::ShowBookmarkAppDialog(
        initiator_web_contents, *web_app_info,
        base::BindOnce(BookmarkAppAcceptanceCallback,
                       std::move(web_app_acceptance_callback)));
  }
}

WebAppInstalledCallback& GetInstalledCallbackForTesting() {
  static base::NoDestructor<WebAppInstalledCallback> instance;
  return *instance;
}

void OnWebAppInstalled(WebAppInstalledCallback callback,
                       const AppId& installed_app_id,
                       InstallResultCode code) {
  if (GetInstalledCallbackForTesting())
    std::move(GetInstalledCallbackForTesting()).Run(installed_app_id, code);

  std::move(callback).Run(installed_app_id, code);
}

}  // namespace

bool CanCreateWebApp(const Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return false;

  return provider->install_manager().CanInstallWebApp(web_contents);
}

void CreateWebAppFromCurrentWebContents(Browser* browser,
                                        bool force_shortcut_app) {
  DCHECK(CanCreateWebApp(browser));

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  DCHECK(provider);

  WebAppInstalledCallback installed_callback = base::DoNothing();

  provider->install_manager().InstallWebApp(
      web_contents, force_shortcut_app,
      InstallableMetrics::GetInstallSource(web_contents, InstallTrigger::MENU),
      base::BindOnce(WebAppInstallDialogCallback),
      base::BindOnce(OnWebAppInstalled, std::move(installed_callback)));
}

bool CreateWebAppFromBanner(content::WebContents* web_contents,
                            WebappInstallSource install_source,
                            WebAppInstalledCallback installed_callback) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return false;

  provider->install_manager().InstallWebAppFromBanner(
      web_contents, install_source, base::BindOnce(WebAppInstallDialogCallback),
      base::BindOnce(OnWebAppInstalled, std::move(installed_callback)));
  return true;
}

void SetInstalledCallbackForTesting(WebAppInstalledCallback callback) {
  GetInstalledCallbackForTesting() = std::move(callback);
}

}  // namespace web_app
