// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_uninstaller.h"

#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"

namespace extensions {

BookmarkAppUninstaller::BookmarkAppUninstaller(Profile* profile,
                                               web_app::AppRegistrar* registrar)
    : profile_(profile),
      registrar_(registrar),
      externally_installed_app_prefs_(profile->GetPrefs()) {}

BookmarkAppUninstaller::~BookmarkAppUninstaller() = default;

void BookmarkAppUninstaller::UninstallApp(const GURL& app_url,
                                          UninstallCallback callback) {
  base::Optional<web_app::AppId> app_id =
      externally_installed_app_prefs_.LookupAppId(app_url);
  if (!app_id.has_value()) {
    LOG(WARNING) << "Couldn't uninstall app with url " << app_url
                 << "; No corresponding extension for url.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  if (!registrar_->IsInstalled(app_id.value())) {
    LOG(WARNING) << "Couldn't uninstall app with url " << app_url
                 << "; Extension not installed.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  base::string16 error;
  bool uninstalled =
      ExtensionSystem::Get(profile_)->extension_service()->UninstallExtension(
          app_id.value(), UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION, &error);

  if (!uninstalled) {
    LOG(WARNING) << "Couldn't uninstall app with url " << app_url << ". "
                 << error;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), uninstalled));
}

}  // namespace extensions
