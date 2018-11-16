// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

WebAppInstallManager::WebAppInstallManager(Profile* profile,
                                           WebAppRegistrar* registrar)
    : profile_(profile),
      registrar_(registrar),
      data_retriever_(std::make_unique<WebAppDataRetriever>()) {
  DCHECK(AllowWebAppInstallation(profile_));
}

WebAppInstallManager::~WebAppInstallManager() = default;

bool WebAppInstallManager::CanInstallWebApp(
    const content::WebContents* web_contents) {
  return IsValidWebAppUrl(web_contents->GetURL());
}

void WebAppInstallManager::InstallWebApp(content::WebContents* contents,
                                         bool force_shortcut_app,
                                         OnceInstallCallback install_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Concurrent calls are not allowed.
  DCHECK(!web_contents());
  CHECK(!install_callback_);

  Observe(contents);
  install_callback_ = std::move(install_callback);

  data_retriever_->GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallManager::OnGetWebApplicationInfo,
                     weak_ptr_factory_.GetWeakPtr(), force_shortcut_app));
}

void WebAppInstallManager::WebContentsDestroyed() {
  ReturnError(InstallResultCode::kWebContentsDestroyed);
}

void WebAppInstallManager::SetDataRetrieverForTesting(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  data_retriever_ = std::move(data_retriever);
}

void WebAppInstallManager::CallInstallCallback(const AppId& app_id,
                                               InstallResultCode code) {
  Observe(nullptr);

  DCHECK(install_callback_);
  std::move(install_callback_).Run(app_id, code);
}

void WebAppInstallManager::ReturnError(InstallResultCode code) {
  CallInstallCallback(AppId(), code);
}

bool WebAppInstallManager::InstallInterrupted() const {
  // Interrupt early if WebContents is being destroyed.
  // WebContentsDestroyed will get called eventually and the callback will be
  // invoked at that point.
  if (!web_contents() || web_contents()->IsBeingDestroyed())
    return true;

  return false;
}

void WebAppInstallManager::OnGetWebApplicationInfo(
    bool force_shortcut_app,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If interrupted, install_callback_ is already invoked or may invoke later.
  if (InstallInterrupted())
    return;

  if (!web_app_info)
    return ReturnError(InstallResultCode::kGetWebApplicationInfoFailed);

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents(),
      base::BindOnce(&WebAppInstallManager::OnDidPerformInstallableCheck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app_info),
                     force_shortcut_app));
}

void WebAppInstallManager::OnDidPerformInstallableCheck(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    bool force_shortcut_app,
    const blink::Manifest& manifest,
    bool is_installable) {
  // If interrupted, install_callback_ is already invoked or may invoke later.
  if (InstallInterrupted())
    return;

  DCHECK(web_app_info);

  const ForInstallableSite for_installable_site =
      is_installable && !force_shortcut_app ? ForInstallableSite::kYes
                                            : ForInstallableSite::kNo;

  // TODO(loyso): Implement installation logic from BookmarkAppHelper:
  // - UpdateShareTargetInPrefs.
  // - WebAppIconDownloader.
  // etc

  UpdateWebAppInfoFromManifest(manifest, web_app_info.get(),
                               for_installable_site);

  const AppId app_id = GenerateAppIdFromURL(web_app_info->app_url);
  auto web_app = std::make_unique<WebApp>(app_id);

  web_app->SetName(base::UTF16ToUTF8(web_app_info->title));
  web_app->SetDescription(base::UTF16ToUTF8(web_app_info->description));
  web_app->SetLaunchUrl(web_app_info->app_url);
  web_app->SetScope(web_app_info->scope);
  web_app->SetThemeColor(web_app_info->theme_color);

  registrar_->RegisterApp(std::move(web_app));

  CallInstallCallback(app_id, InstallResultCode::kSuccess);
}

}  // namespace web_app
