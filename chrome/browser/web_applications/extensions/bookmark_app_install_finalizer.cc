// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace extensions {

BookmarkAppInstallFinalizer::BookmarkAppInstallFinalizer(Profile* profile)
    : profile_(profile) {}

BookmarkAppInstallFinalizer::~BookmarkAppInstallFinalizer() = default;

void BookmarkAppInstallFinalizer::FinalizeInstall(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallFinalizedCallback callback) {
  // Concurrent calls are not allowed.
  DCHECK(!web_app_info_);

  if (!crx_installer_) {
    ExtensionService* extension_service =
        ExtensionSystem::Get(profile_)->extension_service();
    DCHECK(extension_service);
    crx_installer_ = CrxInstaller::CreateSilent(extension_service);
  }

  web_app_info_ = std::move(web_app_info);

  crx_installer_->set_installer_callback(
      base::BindOnce(&BookmarkAppInstallFinalizer::OnExtensionInstalled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     web_app_info_->app_url));
  crx_installer_->InstallWebApp(*web_app_info_);
}

void BookmarkAppInstallFinalizer::SetCrxInstallerForTesting(
    scoped_refptr<CrxInstaller> crx_installer) {
  crx_installer_ = crx_installer;
}

void BookmarkAppInstallFinalizer::OnExtensionInstalled(
    InstallFinalizedCallback callback,
    const GURL& app_url,
    const base::Optional<CrxInstallError>& error) {
  DCHECK(web_app_info_);

  if (error) {
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kFailedUnknownReason);
  } else {
    auto* extension = crx_installer_->extension();
    DCHECK(extension);
    DCHECK_EQ(AppLaunchInfo::GetLaunchWebURL(extension), app_url);

    LaunchType launch_type = web_app_info_->open_as_window
                                 ? LAUNCH_TYPE_WINDOW
                                 : LAUNCH_TYPE_REGULAR;

    // Set the launcher type for the app.
    SetLaunchType(profile_, extension->id(), launch_type);

    // Set this app to be locally installed, as it was installed from this
    // machine.
    SetBookmarkAppIsLocallyInstalled(profile_, extension, true);

    std::move(callback).Run(extension->id(),
                            web_app::InstallResultCode::kSuccess);
  }

  web_app_info_ = nullptr;
}

}  // namespace extensions
