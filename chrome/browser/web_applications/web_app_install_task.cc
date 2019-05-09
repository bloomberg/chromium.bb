// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_task.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace web_app {

WebAppInstallTask::WebAppInstallTask(
    Profile* profile,
    InstallFinalizer* install_finalizer,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : data_retriever_(std::move(data_retriever)),
      install_finalizer_(install_finalizer),
      profile_(profile) {}

WebAppInstallTask::~WebAppInstallTask() = default;

void WebAppInstallTask::InstallWebAppFromManifest(
    content::WebContents* contents,
    WebappInstallSource install_source,
    InstallManager::WebAppInstallDialogCallback dialog_callback,
    InstallManager::OnceInstallCallback install_callback) {
  CheckInstallPreconditions();

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  auto web_app_info = std::make_unique<WebApplicationInfo>();

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnDidPerformInstallableCheck,
                     base::Unretained(this), std::move(web_app_info),
                     /*force_shortcut_app=*/false));
}

void WebAppInstallTask::InstallWebAppFromManifestWithFallback(
    content::WebContents* contents,
    bool force_shortcut_app,
    WebappInstallSource install_source,
    InstallManager::WebAppInstallDialogCallback dialog_callback,
    InstallManager::OnceInstallCallback install_callback) {
  CheckInstallPreconditions();

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  data_retriever_->GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnGetWebApplicationInfo,
                     base::Unretained(this), force_shortcut_app));
}

void WebAppInstallTask::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    bool no_network_install,
    WebappInstallSource install_source,
    InstallManager::OnceInstallCallback callback) {
  std::vector<BitmapAndSource> square_icons;
  FilterSquareIconsFromInfo(*web_application_info, &square_icons);
  ResizeDownloadedIconsGenerateMissing(std::move(square_icons),
                                       web_application_info.get());

  install_source_ = install_source;
  // |for_installable_site| arg is determined by fetching a manifest and running
  // the eligibility check on it. If we don't hit the network, assume that the
  // app represetned by |web_application_info| is installable.
  RecordInstallEvent(no_network_install ? ForInstallableSite::kYes
                                        : ForInstallableSite::kNo);

  InstallFinalizer::FinalizeOptions options;
  if (no_network_install) {
    options.no_network_install = true;
    // We should only install windowed apps via this method.
    options.force_launch_container = LaunchContainer::kWindow;
  }

  install_finalizer_->FinalizeInstall(*web_application_info, options,
                                      std::move(callback));
}

void WebAppInstallTask::WebContentsDestroyed() {
  CallInstallCallback(AppId(), InstallResultCode::kWebContentsDestroyed);
}

void WebAppInstallTask::SetInstallFinalizerForTesting(
    InstallFinalizer* install_finalizer) {
  install_finalizer_ = install_finalizer;
}

void WebAppInstallTask::CheckInstallPreconditions() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(AreWebAppsUserInstallable(profile_));

  // Concurrent calls are not allowed.
  DCHECK(!web_contents());
  CHECK(!install_callback_);
}

void WebAppInstallTask::RecordInstallEvent(
    ForInstallableSite for_installable_site) {
  DCHECK(install_source_ != kNoInstallSource);

  if (InstallableMetrics::IsReportableInstallSource(install_source_) &&
      for_installable_site == ForInstallableSite::kYes) {
    InstallableMetrics::TrackInstallEvent(install_source_);
  }
}

void WebAppInstallTask::CallInstallCallback(const AppId& app_id,
                                            InstallResultCode code) {
  Observe(nullptr);
  dialog_callback_.Reset();

  DCHECK(install_source_ != kNoInstallSource);
  install_source_ = kNoInstallSource;

  DCHECK(install_callback_);
  std::move(install_callback_).Run(app_id, code);
}

bool WebAppInstallTask::ShouldStopInstall() const {
  // Install should stop early if WebContents is being destroyed.
  // WebAppInstallTask::WebContentsDestroyed will get called eventually and the
  // callback will be invoked at that point.
  return !web_contents() || web_contents()->IsBeingDestroyed();
}

void WebAppInstallTask::OnGetWebApplicationInfo(
    bool force_shortcut_app,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (ShouldStopInstall())
    return;

  if (!web_app_info) {
    CallInstallCallback(AppId(),
                        InstallResultCode::kGetWebApplicationInfoFailed);
    return;
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnDidPerformInstallableCheck,
                     base::Unretained(this), std::move(web_app_info),
                     force_shortcut_app));
}

void WebAppInstallTask::OnDidPerformInstallableCheck(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    bool force_shortcut_app,
    const blink::Manifest& manifest,
    bool is_installable) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  const auto for_installable_site = is_installable && !force_shortcut_app
                                        ? ForInstallableSite::kYes
                                        : ForInstallableSite::kNo;

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
      base::BindOnce(&WebAppInstallTask::OnIconsRetrieved,
                     base::Unretained(this), std::move(web_app_info),
                     for_installable_site));
}

void WebAppInstallTask::OnIconsRetrieved(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    IconsMap icons_map) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  std::vector<BitmapAndSource> downloaded_icons =
      FilterSquareIcons(icons_map, *web_app_info);
  ResizeDownloadedIconsGenerateMissing(std::move(downloaded_icons),
                                       web_app_info.get());

  std::move(dialog_callback_)
      .Run(
          web_contents(), std::move(web_app_info), for_installable_site,
          base::BindOnce(&WebAppInstallTask::OnDialogCompleted,
                         weak_ptr_factory_.GetWeakPtr(), for_installable_site));
}

void WebAppInstallTask::OnDialogCompleted(
    ForInstallableSite for_installable_site,
    bool user_accepted,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  if (ShouldStopInstall())
    return;

  if (!user_accepted) {
    CallInstallCallback(AppId(), InstallResultCode::kUserInstallDeclined);
    return;
  }

  WebApplicationInfo web_app_info_copy = *web_app_info;

  // This metric is recorded regardless of the installation result.
  RecordInstallEvent(for_installable_site);

  InstallFinalizer::FinalizeOptions options;

  install_finalizer_->FinalizeInstall(
      web_app_info_copy, options,
      base::BindOnce(&WebAppInstallTask::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app_info)));

  // Check that the finalizer hasn't called OnInstallFinalized synchronously:
  DCHECK(install_callback_);
}

void WebAppInstallTask::OnInstallFinalized(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    const AppId& app_id,
    InstallResultCode code) {
  if (ShouldStopInstall())
    return;

  if (code != InstallResultCode::kSuccess) {
    CallInstallCallback(app_id, code);
    return;
  }

  RecordAppBanner(web_contents(), web_app_info->app_url);

  // TODO(loyso): Implement |create_shortcuts| to skip OS shortcuts creation.
  auto create_shortcuts_callback = base::BindOnce(
      &WebAppInstallTask::OnShortcutsCreated, weak_ptr_factory_.GetWeakPtr(),
      std::move(web_app_info), app_id);
  if (install_finalizer_->CanCreateOsShortcuts()) {
    install_finalizer_->CreateOsShortcuts(app_id, /*add_to_dekstop=*/true,
                                          std::move(create_shortcuts_callback));
  } else {
    std::move(create_shortcuts_callback).Run(false /* created_shortcuts */);
  }
}

void WebAppInstallTask::OnShortcutsCreated(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    const AppId& app_id,
    bool shortcut_created) {
  if (ShouldStopInstall())
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
