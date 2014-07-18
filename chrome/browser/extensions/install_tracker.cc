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
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"

namespace extensions {

InstallTracker::InstallTracker(Profile* profile,
                               extensions::ExtensionPrefs* prefs)
    : extension_registry_observer_(this) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_APP_INSTALLED_TO_APPLIST,
      content::Source<Profile>(profile));
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile));

  // Prefs may be null in tests.
  if (prefs) {
    AppSorting* sorting = prefs->app_sorting();
    registrar_.Add(this,
                   chrome::NOTIFICATION_EXTENSION_LAUNCHER_REORDERED,
                   content::Source<AppSorting>(sorting));
    pref_change_registrar_.Init(prefs->pref_service());
    pref_change_registrar_.Add(
        pref_names::kExtensions,
        base::Bind(&InstallTracker::OnAppsReordered, base::Unretained(this)));
  }
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

const ActiveInstallData* InstallTracker::GetActiveInstall(
    const std::string& extension_id) const {
  ActiveInstallsMap::const_iterator install_data =
      active_installs_.find(extension_id);
  if (install_data == active_installs_.end())
    return NULL;
  else
    return &(install_data->second);
}

void InstallTracker::AddActiveInstall(const ActiveInstallData& install_data) {
  DCHECK(!install_data.extension_id.empty());
  DCHECK(active_installs_.find(install_data.extension_id) ==
         active_installs_.end());
  active_installs_.insert(
      std::make_pair(install_data.extension_id, install_data));
}

void InstallTracker::RemoveActiveInstall(const std::string& extension_id) {
  active_installs_.erase(extension_id);
}

void InstallTracker::OnBeginExtensionInstall(
    const InstallObserver::ExtensionInstallParams& params) {
  ActiveInstallsMap::iterator install_data =
      active_installs_.find(params.extension_id);
  if (install_data == active_installs_.end()) {
    ActiveInstallData install_data(params.extension_id);
    install_data.is_ephemeral = params.is_ephemeral;
    active_installs_.insert(std::make_pair(params.extension_id, install_data));
  }

  FOR_EACH_OBSERVER(InstallObserver, observers_,
                    OnBeginExtensionInstall(params));
}

void InstallTracker::OnBeginExtensionDownload(const std::string& extension_id) {
  FOR_EACH_OBSERVER(
      InstallObserver, observers_, OnBeginExtensionDownload(extension_id));
}

void InstallTracker::OnDownloadProgress(const std::string& extension_id,
                                        int percent_downloaded) {
  ActiveInstallsMap::iterator install_data =
      active_installs_.find(extension_id);
  if (install_data != active_installs_.end()) {
    install_data->second.percent_downloaded = percent_downloaded;
  } else {
    NOTREACHED();
  }

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
  RemoveActiveInstall(extension_id);
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

void InstallTracker::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  RemoveActiveInstall(extension->id());
}

void InstallTracker::OnAppsReordered() {
  FOR_EACH_OBSERVER(InstallObserver, observers_, OnAppsReordered());
}

}  // namespace extensions
