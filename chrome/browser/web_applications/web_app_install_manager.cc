// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_manager.h"
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

  // No parrallel installations support.
  DCHECK(!web_contents());

  force_shortcut_app_ = force_shortcut_app;
  install_callback_ = std::move(install_callback);
  Observe(contents);

  data_retriever_->GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallManager::OnGetWebApplicationInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppInstallManager::WebContentsDestroyed() {
  ReturnError(InstallResultCode::kWebContentsDestroyed);
}

void WebAppInstallManager::SetDataRetrieverForTesting(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  data_retriever_ = std::move(data_retriever);
}

void WebAppInstallManager::ResetInstallProcessArguments() {
  Observe(nullptr);
  force_shortcut_app_ = false;
  web_app_info_.reset();
}

void WebAppInstallManager::CallInstallCallback(const AppId& app_id,
                                               InstallResultCode code) {
  ResetInstallProcessArguments();

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
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If interrupted, install_callback_ is already invoked or may invoke later.
  if (InstallInterrupted())
    return;

  if (!web_app_info)
    return ReturnError(InstallResultCode::kGetWebApplicationInfoFailed);

  web_app_info_ = std::move(web_app_info);

  // TODO(loyso): Consider to merge InstallableManager into WebAppDataRetriever.
  InstallableManager* installable_manager =
      InstallableManager::FromWebContents(web_contents());
  DCHECK(installable_manager);

  // TODO(crbug.com/829232) Unify with other calls to GetData.
  InstallableParams params;
  params.check_eligibility = true;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.has_worker = true;
  // Do not wait_for_worker. OnDidPerformInstallableCheck is always invoked.
  installable_manager->GetData(
      params,
      base::BindRepeating(&WebAppInstallManager::OnDidPerformInstallableCheck,
                          weak_ptr_factory_.GetWeakPtr()));
}

void WebAppInstallManager::OnDidPerformInstallableCheck(
    const InstallableData& data) {
  DCHECK(data.manifest_url.is_valid() || data.manifest->IsEmpty());
  // If interrupted, install_callback_ is already invoked or may invoke later.
  if (InstallInterrupted())
    return;

  const ForInstallableSite for_installable_site =
      data.error_code == NO_ERROR_DETECTED && !force_shortcut_app_
          ? ForInstallableSite::kYes
          : ForInstallableSite::kNo;

  UpdateWebAppInfoFromManifest(*data.manifest, web_app_info_.get(),
                               for_installable_site);

  // TODO(loyso): Implement installation logic from BookmarkAppHelper:
  // - UpdateShareTargetInPrefs.
  // - WebAppIconDownloader.
  // etc

  const AppId app_id = GenerateAppIdFromURL(web_app_info_->app_url);
  auto web_app = std::make_unique<WebApp>(app_id);

  web_app->SetName(base::UTF16ToUTF8(web_app_info_->title));
  web_app->SetDescription(base::UTF16ToUTF8(web_app_info_->description));
  web_app->SetLaunchUrl(web_app_info_->app_url.spec());

  registrar_->RegisterApp(std::move(web_app));

  CallInstallCallback(app_id, InstallResultCode::kSuccess);
}

}  // namespace web_app
