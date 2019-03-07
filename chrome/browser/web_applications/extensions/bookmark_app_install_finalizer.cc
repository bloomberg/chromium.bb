// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
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
#include "extensions/browser/install/crx_install_error.h"
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

void OnExtensionInstalled(
    const GURL& app_url,
    LaunchType launch_type,
    web_app::InstallFinalizer::InstallFinalizedCallback callback,
    scoped_refptr<CrxInstaller> crx_installer,
    const base::Optional<CrxInstallError>& error) {
  if (error) {
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kFailedUnknownReason);
    return;
  }

  const Extension* extension = crx_installer->extension();
  DCHECK(extension);

  DCHECK_EQ(AppLaunchInfo::GetLaunchWebURL(extension), app_url);

  // Set the launcher type for the app.
  SetLaunchType(crx_installer->profile(), extension->id(), launch_type);

  // Set this app to be locally installed, as it was installed from this
  // machine.
  SetBookmarkAppIsLocallyInstalled(crx_installer->profile(), extension, true);

  std::move(callback).Run(extension->id(),
                          web_app::InstallResultCode::kSuccess);
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
  scoped_refptr<CrxInstaller> crx_installer =
      crx_installer_factory_.Run(profile_);

  crx_installer->set_installer_callback(base::BindOnce(
      OnExtensionInstalled, web_app_info.app_url, GetLaunchType(web_app_info),
      std::move(callback), crx_installer));

  crx_installer->InstallWebApp(web_app_info);
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

bool BookmarkAppInstallFinalizer::CanReparentTab(const web_app::AppId& app_id,
                                                 bool shortcut_created) const {
  const Extension* app = GetExtensionById(profile_, app_id);
  return CanBookmarkAppReparentTab(profile_, app, shortcut_created);
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
