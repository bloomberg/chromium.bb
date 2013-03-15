// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_tracker.h"

#include "base/bind.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

InstallTracker::InstallTracker(Profile* profile,
                               extensions::ExtensionPrefs* prefs) {
  ExtensionSorting* sorting = prefs->extension_sorting();

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LAUNCHER_REORDERED,
      content::Source<ExtensionSorting>(sorting));
  registrar_.Add(this, chrome::NOTIFICATION_APP_INSTALLED_TO_APPLIST,
      content::Source<Profile>(profile));

  pref_change_registrar_.Init(prefs->pref_service());
  pref_change_registrar_.Add(extensions::ExtensionPrefs::kExtensionsPref,
                             base::Bind(&InstallTracker::OnAppsReordered,
                                        base::Unretained(this)));
}

InstallTracker::~InstallTracker() {
}

void InstallTracker::AddObserver(InstallObserver* observer) {
  observers_.AddObserver(observer);
}

void InstallTracker::RemoveObserver(InstallObserver* observer) {
  observers_.RemoveObserver(observer);
}

void InstallTracker::OnBeginExtensionInstall(
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon,
    bool is_app,
    bool is_platform_app) {
  FOR_EACH_OBSERVER(InstallObserver, observers_,
                    OnBeginExtensionInstall(extension_id,
                                            extension_name,
                                            installing_icon,
                                            is_app,
                                            is_platform_app));
}

void InstallTracker::OnDownloadProgress(const std::string& extension_id,
                                        int percent_downloaded) {
  FOR_EACH_OBSERVER(InstallObserver, observers_,
                    OnDownloadProgress(extension_id, percent_downloaded));
}

void InstallTracker::OnInstallFailure(
    const std::string& extension_id) {
  FOR_EACH_OBSERVER(InstallObserver, observers_,
                    OnInstallFailure(extension_id));
}

void InstallTracker::Shutdown() {
  FOR_EACH_OBSERVER(InstallObserver, observers_, OnShutdown());
}

void InstallTracker::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      FOR_EACH_OBSERVER(InstallObserver, observers_,
                        OnExtensionInstalled(extension));
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const content::Details<extensions::UnloadedExtensionInfo>& unload_info(
          details);
      const Extension* extension = unload_info->extension;
      if (unload_info->reason == extension_misc::UNLOAD_REASON_UNINSTALL) {
        FOR_EACH_OBSERVER(InstallObserver, observers_,
                          OnExtensionUninstalled(extension));
      } else {
        FOR_EACH_OBSERVER(InstallObserver, observers_,
                          OnExtensionDisabled(extension));
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LAUNCHER_REORDERED: {
      FOR_EACH_OBSERVER(InstallObserver, observers_, OnAppsReordered());
      break;
    }
    case chrome::NOTIFICATION_APP_INSTALLED_TO_APPLIST: {
      const std::string& extension_id(
          *content::Details<const std::string>(details).ptr());
      FOR_EACH_OBSERVER(InstallObserver, observers_,
                        OnAppInstalledToAppList(extension_id));
      break;
    }
    default:
      NOTREACHED();
  }
}

void InstallTracker::OnAppsReordered() {
  FOR_EACH_OBSERVER(InstallObserver, observers_, OnAppsReordered());
}

}  // namespace extensions
