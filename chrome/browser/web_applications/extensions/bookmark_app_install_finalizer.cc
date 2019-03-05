// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/extensions/bookmark_app_extension_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "url/gurl.h"

namespace extensions {

namespace {

LaunchType GetLaunchType(const WebApplicationInfo& web_app_info) {
  return web_app_info.open_as_window ? LAUNCH_TYPE_WINDOW : LAUNCH_TYPE_REGULAR;
}

const Extension* GetExtensionById(Profile* profile,
                                  const web_app::AppId& app_id) {
  const Extension* app =
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(app_id);
  DCHECK(app);
  return app;
}

}  // namespace

BookmarkAppInstallFinalizer::BookmarkAppInstallFinalizer(Profile* profile)
    : profile_(profile) {
  crx_installer_factory_ = base::BindRepeating([](Profile* profile) {
    ExtensionService* extension_service =
        ExtensionSystem::Get(profile)->extension_service();
    DCHECK(extension_service);
    return CrxInstaller::CreateSilent(extension_service);
  });
}

BookmarkAppInstallFinalizer::~BookmarkAppInstallFinalizer() = default;

void BookmarkAppInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    InstallFinalizedCallback callback) {
  DCHECK(!crx_installer_);

  crx_installer_ = crx_installer_factory_.Run(profile_);
  DCHECK(crx_installer_);

  crx_installer_->set_installer_callback(base::BindOnce(
      &BookmarkAppInstallFinalizer::OnExtensionInstalled,
      weak_ptr_factory_.GetWeakPtr(),
      std::make_unique<WebApplicationInfo>(web_app_info), std::move(callback)));
  crx_installer_->InstallWebApp(web_app_info);
}

void BookmarkAppInstallFinalizer::OnExtensionInstalled(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallFinalizedCallback callback,
    const base::Optional<CrxInstallError>& error) {
  const Extension* extension = crx_installer_->extension();
  crx_installer_.reset();

  if (error) {
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kFailedUnknownReason);
    return;
  }

  DCHECK(extension);
  DCHECK_EQ(AppLaunchInfo::GetLaunchWebURL(extension), web_app_info->app_url);

  const LaunchType launch_type = GetLaunchType(*web_app_info);

  // Set the launcher type for the app.
  SetLaunchType(profile_, extension->id(), launch_type);

  // Set this app to be locally installed, as it was installed from this
  // machine.
  SetBookmarkAppIsLocallyInstalled(profile_, extension, true);

  std::move(callback).Run(extension->id(),
                          web_app::InstallResultCode::kSuccess);
}

bool BookmarkAppInstallFinalizer::CanCreateOsShortcuts() const {
  return CanBookmarkAppCreateOsShortcuts();
}

void BookmarkAppInstallFinalizer::CreateOsShortcuts(
    const web_app::AppId& app_id,
    CreateOsShortcutsCallback callback) {
  const Extension* app = GetExtensionById(profile_, app_id);
  BookmarkAppCreateOsShortcuts(profile_, app, std::move(callback));
}

bool BookmarkAppInstallFinalizer::CanPinAppToShelf() const {
  return CanBookmarkAppBePinnedToShelf();
}

void BookmarkAppInstallFinalizer::PinAppToShelf(const web_app::AppId& app_id) {
  const Extension* app = GetExtensionById(profile_, app_id);
  BookmarkAppPinToShelf(app);
}

bool BookmarkAppInstallFinalizer::CanReparentTab(bool shortcut_created) const {
  return CanBookmarkAppReparentTab(shortcut_created);
}

void BookmarkAppInstallFinalizer::ReparentTab(
    const web_app::AppId& app_id,
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  DCHECK(!profile_->IsOffTheRecord());
  const Extension* app = GetExtensionById(profile_, app_id);
  BookmarkAppReparentTab(web_contents, app);
}

bool BookmarkAppInstallFinalizer::CanRevealAppShim() const {
  return CanBookmarkAppRevealAppShim();
}

void BookmarkAppInstallFinalizer::RevealAppShim(const web_app::AppId& app_id) {
  const Extension* app = GetExtensionById(profile_, app_id);
  BookmarkAppRevealAppShim(profile_, app);
}

void BookmarkAppInstallFinalizer::SetCrxInstallerFactoryForTesting(
    CrxInstallerFactory crx_installer_factory) {
  crx_installer_factory_ = crx_installer_factory;
}

}  // namespace extensions
