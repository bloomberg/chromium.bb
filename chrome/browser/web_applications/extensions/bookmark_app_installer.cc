// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/web_application_info.h"

namespace extensions {

BookmarkAppInstaller::BookmarkAppInstaller(Profile* profile)
    : crx_installer_(CrxInstaller::CreateSilent(
          ExtensionSystem::Get(profile)->extension_service())) {}

BookmarkAppInstaller::~BookmarkAppInstaller() = default;

void BookmarkAppInstaller::Install(const WebApplicationInfo& web_app_info,
                                   ResultCallback callback) {
  crx_installer_->set_installer_callback(
      base::BindOnce(&BookmarkAppInstaller::OnInstall,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  crx_installer_->InstallWebApp(web_app_info);
}

void BookmarkAppInstaller::SetCrxInstallerForTesting(
    scoped_refptr<CrxInstaller> crx_installer) {
  crx_installer_ = crx_installer;
}

void BookmarkAppInstaller::OnInstall(
    ResultCallback callback,
    const base::Optional<CrxInstallError>& error) {
  // TODO(crbug.com/864904): Finish the installation i.e. set launch container.

  const bool installation_succeeded = !error;
  std::move(callback).Run(installation_succeeded);
}

}  // namespace extensions
