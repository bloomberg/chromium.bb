// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace web_app {

WebAppInstallManager::WebAppInstallManager(
    Profile* profile,
    std::unique_ptr<InstallFinalizer> install_finalizer)
    : data_retriever_(std::make_unique<WebAppDataRetriever>()),
      install_finalizer_(std::move(install_finalizer)) {
  DCHECK(AllowWebAppInstallation(profile));
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

void WebAppInstallManager::SetInstallFinalizerForTesting(
    std::unique_ptr<InstallFinalizer> install_finalizer) {
  install_finalizer_ = std::move(install_finalizer);
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

  // TODO(loyso): Implement UpdateShareTargetInPrefs installation logic.

  UpdateWebAppInfoFromManifest(manifest, web_app_info.get(),
                               for_installable_site);

  std::vector<GURL> icon_urls;
  for (auto& icon_info : web_app_info->icons) {
    if (icon_info.url.is_valid())
      icon_urls.push_back(icon_info.url);
  }

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_fav_icons = !manifest.icons.empty();

  data_retriever_->GetIcons(
      web_contents(), icon_urls, skip_page_fav_icons,
      base::BindOnce(&WebAppInstallManager::OnIconsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app_info)));
}

void WebAppInstallManager::OnIconsRetrieved(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    IconsMap icons_map) {
  // If interrupted, install_callback_ is already invoked or may invoke later.
  if (InstallInterrupted())
    return;

  DCHECK(web_app_info);

  std::vector<BitmapAndSource> downloaded_icons =
      FilterSquareIcons(icons_map, *web_app_info);
  ResizeDownloadedIconsGenerateMissing(std::move(downloaded_icons),
                                       web_app_info.get());

  install_finalizer_->FinalizeInstall(
      std::move(web_app_info),
      base::BindOnce(&WebAppInstallManager::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppInstallManager::OnInstallFinalized(const AppId& app_id,
                                              InstallResultCode code) {
  CallInstallCallback(app_id, code);
}

}  // namespace web_app
