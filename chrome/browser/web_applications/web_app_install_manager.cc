// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
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
      install_finalizer_(std::move(install_finalizer)),
      profile_(profile) {}

WebAppInstallManager::~WebAppInstallManager() = default;

bool WebAppInstallManager::CanInstallWebApp(
    content::WebContents* web_contents) {
  Profile* web_contents_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  return AreWebAppsUserInstallable(web_contents_profile) &&
         IsValidWebAppUrl(web_contents->GetLastCommittedURL());
}

void WebAppInstallManager::InstallWebApp(
    content::WebContents* contents,
    bool force_shortcut_app,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback install_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(AreWebAppsUserInstallable(profile_));

  // Concurrent calls are not allowed.
  DCHECK(!web_contents());
  CHECK(!install_callback_);

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  data_retriever_->GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallManager::OnGetWebApplicationInfo,
                     weak_ptr_factory_.GetWeakPtr(), force_shortcut_app));
}

void WebAppInstallManager::InstallWebAppFromBanner(
    content::WebContents* contents,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  // TODO(loyso): Implement it.
  NOTIMPLEMENTED();
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
  dialog_callback_.Reset();

  DCHECK(install_source_ != kNoInstallSource);
  install_source_ = kNoInstallSource;

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
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app_info),
                     for_installable_site));
}

void WebAppInstallManager::OnIconsRetrieved(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    IconsMap icons_map) {
  if (InstallInterrupted())
    return;

  DCHECK(web_app_info);

  std::vector<BitmapAndSource> downloaded_icons =
      FilterSquareIcons(icons_map, *web_app_info);
  ResizeDownloadedIconsGenerateMissing(std::move(downloaded_icons),
                                       web_app_info.get());

  std::move(dialog_callback_)
      .Run(
          web_contents(), std::move(web_app_info), for_installable_site,
          base::BindOnce(&WebAppInstallManager::OnDialogCompleted,
                         weak_ptr_factory_.GetWeakPtr(), for_installable_site));
}

void WebAppInstallManager::OnDialogCompleted(
    ForInstallableSite for_installable_site,
    bool user_accepted,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  if (InstallInterrupted())
    return;

  if (!user_accepted)
    return ReturnError(InstallResultCode::kUserInstallDeclined);

  WebApplicationInfo web_app_info_copy = *web_app_info;

  DCHECK(install_source_ != kNoInstallSource);

  // This metric is recorded regardless of the installation result.
  if (InstallableMetrics::IsReportableInstallSource(install_source_) &&
      for_installable_site == ForInstallableSite::kYes) {
    InstallableMetrics::TrackInstallEvent(install_source_);
  }

  install_finalizer_->FinalizeInstall(
      web_app_info_copy,
      base::BindOnce(&WebAppInstallManager::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app_info)));

  // Check that the finalizer hasn't called OnInstallFinalized synchronously:
  DCHECK(install_callback_);
}

void WebAppInstallManager::OnInstallFinalized(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    const AppId& app_id,
    InstallResultCode code) {
  if (InstallInterrupted())
    return;

  if (code != InstallResultCode::kSuccess) {
    CallInstallCallback(app_id, code);
    return;
  }

  RecordAppBanner(web_contents(), web_app_info->app_url);

  // TODO(loyso): Implement |create_shortcuts| to skip OS shortcuts creation.
  auto create_shortcuts_callback = base::BindOnce(
      &WebAppInstallManager::OnShortcutsCreated, weak_ptr_factory_.GetWeakPtr(),
      std::move(web_app_info), app_id);
  if (install_finalizer_->CanCreateOsShortcuts()) {
    install_finalizer_->CreateOsShortcuts(app_id,
                                          std::move(create_shortcuts_callback));
  } else {
    std::move(create_shortcuts_callback).Run(false /* created_shortcuts */);
  }
}

void WebAppInstallManager::OnShortcutsCreated(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    const AppId& app_id,
    bool shortcut_created) {
  if (InstallInterrupted())
    return;

  if (install_finalizer_->CanPinAppToShelf())
    install_finalizer_->PinAppToShelf(app_id);

  // TODO(loyso): Implement |reparent_tab| to skip tab reparenting logic.
  if (web_app_info->open_as_window &&
      install_finalizer_->CanReparentTab(app_id, shortcut_created)) {
    install_finalizer_->ReparentTab(app_id, web_contents());
  }

  if (install_finalizer_->CanRevealAppShim())
    install_finalizer_->RevealAppShim(app_id);

  CallInstallCallback(app_id, InstallResultCode::kSuccess);
}

}  // namespace web_app
