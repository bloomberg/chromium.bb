// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_restore_service.h"

#include "chrome/browser/extensions/api/app_runtime/app_runtime_api.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

#if defined(OS_WIN)
#include "win8/util/win8_util.h"
#endif

using extensions::AppEventRouter;
using extensions::Extension;
using extensions::ExtensionHost;
using extensions::ExtensionPrefs;
using extensions::ExtensionSystem;

namespace apps {

// static
bool AppRestoreService::ShouldRestoreApps(bool is_browser_restart) {
  bool should_restore_apps = is_browser_restart;
#if defined(OS_CHROMEOS)
  // Chromeos always restarts apps, even if it was a regular shutdown.
  should_restore_apps = true;
#elif defined(OS_WIN)
  // Packaged apps are not supported in Metro mode, so don't try to start them.
  if (win8::IsSingleWindowMetroMode())
    should_restore_apps = false;
#endif
  return should_restore_apps;
}

AppRestoreService::AppRestoreService(Profile* profile)
    : profile_(profile) {
  registrar_.Add(
      this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
      content::NotificationService::AllSources());
  registrar_.Add(
      this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this, chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());
}

void AppRestoreService::HandleStartup(bool should_restore_apps) {
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile_)->extension_service();
  const ExtensionSet* extensions = extension_service->extensions();
  ExtensionPrefs* extension_prefs = extension_service->extension_prefs();

  for (ExtensionSet::const_iterator it = extensions->begin();
      it != extensions->end(); ++it) {
    const Extension* extension = *it;
    if (extension_prefs->IsExtensionRunning(extension->id())) {
      std::vector<SavedFileEntry> file_entries;
      extension_prefs->GetSavedFileEntries(extension->id(), &file_entries);
      RecordAppStop(extension->id());
      if (should_restore_apps)
        RestoreApp(*it, file_entries);
    }
  }
}

void AppRestoreService::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      const Extension* extension = host->extension();
      if (extension && extension->is_platform_app())
        RecordAppStart(extension->id());
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      const Extension* extension = host->extension();
      if (extension && extension->is_platform_app())
        RecordAppStop(extension->id());
      break;
    }

    case chrome::NOTIFICATION_APP_TERMINATING: {
      // Stop listening to NOTIFICATION_EXTENSION_HOST_DESTROYED in particular
      // as all extension hosts will be destroyed as a result of shutdown.
      registrar_.RemoveAll();
      break;
    }
  }
}


void AppRestoreService::RecordAppStart(const std::string& extension_id) {
  ExtensionPrefs* extension_prefs =
      ExtensionSystem::Get(profile_)->extension_service()->extension_prefs();
  extension_prefs->SetExtensionRunning(extension_id, true);
}

void AppRestoreService::RecordAppStop(const std::string& extension_id) {
  ExtensionPrefs* extension_prefs =
      ExtensionSystem::Get(profile_)->extension_service()->extension_prefs();
  extension_prefs->SetExtensionRunning(extension_id, false);
  extension_prefs->ClearSavedFileEntries(extension_id);
}

void AppRestoreService::RestoreApp(
    const Extension* extension,
    const std::vector<SavedFileEntry>& file_entries) {
  extensions::RestartPlatformAppWithFileEntries(profile_,
                                                extension,
                                                file_entries);
}

}  // namespace apps
