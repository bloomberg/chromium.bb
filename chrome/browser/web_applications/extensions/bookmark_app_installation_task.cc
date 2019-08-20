// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

BookmarkAppInstallationTask::Result::Result(
    web_app::InstallResultCode code,
    base::Optional<web_app::AppId> app_id)
    : code(code), app_id(std::move(app_id)) {
  DCHECK_EQ(code == web_app::InstallResultCode::kSuccess, app_id.has_value());
}

BookmarkAppInstallationTask::Result::Result(Result&&) = default;

BookmarkAppInstallationTask::Result::~Result() = default;

// static
void BookmarkAppInstallationTask::CreateTabHelpers(
    content::WebContents* web_contents) {
  InstallableManager::CreateForWebContents(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
}

BookmarkAppInstallationTask::BookmarkAppInstallationTask(
    Profile* profile,
    web_app::AppRegistrar* registrar,
    web_app::InstallFinalizer* install_finalizer,
    web_app::ExternalInstallOptions install_options)
    : profile_(profile),
      registrar_(registrar),
      install_finalizer_(install_finalizer),
      externally_installed_app_prefs_(profile_->GetPrefs()),
      install_options_(std::move(install_options)) {}

BookmarkAppInstallationTask::~BookmarkAppInstallationTask() = default;

void BookmarkAppInstallationTask::Install(
    content::WebContents* web_contents,
    web_app::WebAppUrlLoader::Result load_url_result,
    ResultCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(web_contents->GetBrowserContext(), profile_);

  if (load_url_result == web_app::WebAppUrlLoader::Result::kUrlLoaded) {
    // If we are not re-installing a placeholder, then no need to uninstall
    // anything.
    if (!install_options_.reinstall_placeholder) {
      ContinueWebAppInstall(web_contents, std::move(result_callback));
      return;
    }
    // Calling InstallWebAppWithOptions with the same URL used to install a
    // placeholder won't necessarily replace the placeholder app, because the
    // new app might be installed with a new AppId. To avoid this, always
    // uninstall the placeholder app.
    UninstallPlaceholderApp(web_contents, std::move(result_callback));
    return;
  }

  if (install_options_.install_placeholder) {
    InstallPlaceholder(std::move(result_callback));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback),
                     Result(web_app::InstallResultCode::kFailedUnknownReason,
                            base::nullopt)));
}

void BookmarkAppInstallationTask::UninstallPlaceholderApp(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  base::Optional<web_app::AppId> app_id =
      externally_installed_app_prefs_.LookupPlaceholderAppId(
          install_options_.url);

  // If there is no placeholder app or the app is not installed,
  // then no need to uninstall anything.
  if (!app_id.has_value() || !registrar_->IsInstalled(app_id.value())) {
    ContinueWebAppInstall(web_contents, std::move(result_callback));
    return;
  }

  // Otherwise, uninstall the placeholder app.
  install_finalizer_->UninstallExternalWebApp(
      install_options_.url,
      base::BindOnce(&BookmarkAppInstallationTask::OnPlaceholderUninstalled,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void BookmarkAppInstallationTask::OnPlaceholderUninstalled(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    bool uninstalled) {
  if (!uninstalled) {
    LOG(ERROR) << "Failed to uninstall placeholder for: "
               << install_options_.url;
    std::move(result_callback)
        .Run(Result(web_app::InstallResultCode::kFailedUnknownReason,
                    base::nullopt));
    return;
  }
  ContinueWebAppInstall(web_contents, std::move(result_callback));
}

void BookmarkAppInstallationTask::ContinueWebAppInstall(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile_);
  DCHECK(provider);

  provider->install_manager().InstallWebAppWithOptions(
      web_contents, install_options_,
      base::BindOnce(&BookmarkAppInstallationTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), false /* is_placeholder */,
                     std::move(result_callback)));
}

void BookmarkAppInstallationTask::InstallPlaceholder(ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Optional<web_app::AppId> app_id =
      externally_installed_app_prefs_.LookupPlaceholderAppId(
          install_options_.url);
  if (app_id.has_value() && registrar_->IsInstalled(app_id.value())) {
    // No need to install a placeholder app again.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       Result(web_app::InstallResultCode::kSuccess, app_id)));
    return;
  }

  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(install_options_.url.spec());
  web_app_info.app_url = install_options_.url;

  switch (install_options_.launch_container) {
    case web_app::LaunchContainer::kDefault:
    case web_app::LaunchContainer::kTab:
      web_app_info.open_as_window = false;
      break;
    case web_app::LaunchContainer::kWindow:
      web_app_info.open_as_window = true;
      break;
  }

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::EXTERNAL_POLICY;

  install_finalizer_->FinalizeInstall(
      web_app_info, options,
      base::BindOnce(&BookmarkAppInstallationTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), true /* is_placeholder */,
                     std::move(callback)));
}

void BookmarkAppInstallationTask::OnWebAppInstalled(
    bool is_placeholder,
    ResultCallback result_callback,
    const web_app::AppId& app_id,
    web_app::InstallResultCode code) {
  if (code != web_app::InstallResultCode::kSuccess) {
    std::move(result_callback).Run(Result(code, base::nullopt));
    return;
  }

  externally_installed_app_prefs_.Insert(install_options_.url, app_id,
                                         install_options_.install_source);
  externally_installed_app_prefs_.SetIsPlaceholder(install_options_.url,
                                                   is_placeholder);

  auto success_closure =
      base::BindOnce(std::move(result_callback),
                     Result(web_app::InstallResultCode::kSuccess, app_id));

  if (!is_placeholder) {
    std::move(success_closure).Run();
    return;
  }

  // Installation through InstallFinalizer doesn't create shortcuts so create
  // them here.
  if (install_options_.add_to_quick_launch_bar &&
      install_finalizer_->CanPinAppToShelf()) {
    install_finalizer_->PinAppToShelf(app_id);
  }

  // TODO(ortuno): Make adding a shortcut to the applications menu independent
  // from adding a shortcut to desktop.
  if (install_options_.add_to_applications_menu &&
      install_finalizer_->CanCreateOsShortcuts()) {
    install_finalizer_->CreateOsShortcuts(
        app_id, install_options_.add_to_desktop,
        base::BindOnce(
            [](base::OnceClosure success_closure, bool shortcuts_created) {
              // Even if the shortcuts failed to be created, we consider the
              // installation successful since an app was created.
              std::move(success_closure).Run();
            },
            std::move(success_closure)));
    return;
  }

  std::move(success_closure).Run();
}

}  // namespace extensions
