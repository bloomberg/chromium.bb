// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_tracker.h"

#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"

namespace extensions {

InstallTracker::InstallTracker(Profile* profile,
                               extensions::ExtensionPrefs* prefs) {
  AppSorting* sorting = prefs->app_sorting();

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
      content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
      content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(profile));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LAUNCHER_REORDERED,
      content::Source<AppSorting>(sorting));
  registrar_.Add(this, chrome::NOTIFICATION_APP_INSTALLED_TO_APPLIST,
      content::Source<Profile>(profile));

  pref_change_registrar_.Init(prefs->pref_service());
  pref_change_registrar_.Add(pref_names::kExtensions,
                             base::Bind(&InstallTracker::OnAppsReordered,
                                        base::Unretained(this)));
}

InstallTracker::~InstallTracker() {
}

// static
InstallTracker* InstallTracker::Get(content::BrowserContext* context) {
  return InstallTrackerFactory::GetForProfile(
      Profile::FromBrowserContext(context));
}

void InstallTracker::AddObserver(InstallObserver* observer) {
  observers_.AddObserver(observer);
}

void InstallTracker::RemoveObserver(InstallObserver* observer) {
  observers_.RemoveObserver(observer);
}

void InstallTracker::OnBeginExtensionInstall(
    const InstallObserver::ExtensionInstallParams& params) {
  FOR_EACH_OBSERVER(InstallObserver, observers_,
                    OnBeginExtensionInstall(params));
}

void InstallTracker::OnBeginExtensionDownload(const std::string& extension_id) {
  FOR_EACH_OBSERVER(
      InstallObserver, observers_, OnBeginExtensionDownload(extension_id));
}

void InstallTracker::OnDownloadProgress(const std::string& extension_id,
                                        int percent_downloaded) {
  FOR_EACH_OBSERVER(InstallObserver, observers_,
                    OnDownloadProgress(extension_id, percent_downloaded));
}

void InstallTracker::OnBeginCrxInstall(const std::string& extension_id) {
  FOR_EACH_OBSERVER(
      InstallObserver, observers_, OnBeginCrxInstall(extension_id));
}

void InstallTracker::OnFinishCrxInstall(const std::string& extension_id,
                                        bool success) {
  FOR_EACH_OBSERVER(
      InstallObserver, observers_, OnFinishCrxInstall(extension_id, success));
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
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const Extension* extension =
          content::Details<const InstalledExtensionInfo>(details).ptr()->
              extension;
      FOR_EACH_OBSERVER(InstallObserver, observers_,
                        OnExtensionInstalled(extension));
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      FOR_EACH_OBSERVER(InstallObserver, observers_,
                        OnExtensionLoaded(extension));
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED: {
      const content::Details<extensions::UnloadedExtensionInfo>& unload_info(
          details);
      const Extension* extension = unload_info->extension;
      FOR_EACH_OBSERVER(InstallObserver, observers_,
                        OnExtensionUnloaded(extension));
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();

      FOR_EACH_OBSERVER(InstallObserver, observers_,
                        OnExtensionUninstalled(extension));
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      FOR_EACH_OBSERVER(
          InstallObserver, observers_, OnDisabledExtensionUpdated(extension));
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
