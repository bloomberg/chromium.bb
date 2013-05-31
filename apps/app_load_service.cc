// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_load_service.h"

#include "apps/app_load_service_factory.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

using extensions::Extension;
using extensions::ExtensionPrefs;

namespace {

enum PostReloadAction {
  RELOAD_ACTION_LAUNCH,
  RELOAD_ACTION_RESTART,
};

}  // namespace

namespace apps {

AppLoadService::AppLoadService(Profile* profile)
    : profile_(profile) {
  registrar_.Add(
      this, chrome::NOTIFICATION_EXTENSION_LOADED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::NotificationService::AllSources());
}

AppLoadService::~AppLoadService() {}

void AppLoadService::RestartApplication(const std::string& extension_id) {
  reload_actions_[extension_id] = RELOAD_ACTION_RESTART;
  ExtensionService* service = extensions::ExtensionSystem::Get(profile_)->
      extension_service();
  DCHECK(service);
  service->ReloadExtension(extension_id);
}

void AppLoadService::ScheduleLaunchOnLoad(const std::string& extension_id) {
  reload_actions_[extension_id] = RELOAD_ACTION_LAUNCH;
}

// static
AppLoadService* AppLoadService::Get(Profile* profile) {
  return apps::AppLoadServiceFactory::GetForProfile(profile);
}

void AppLoadService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension = content::Details<Extension>(details).ptr();
      std::map<std::string, int>::iterator it = reload_actions_.find(
          extension->id());
      if (it == reload_actions_.end())
        break;

      switch (it->second) {
        case RELOAD_ACTION_LAUNCH:
          extensions::LaunchPlatformApp(profile_, extension);
          break;
        case RELOAD_ACTION_RESTART:
          extensions::RestartPlatformApp(profile_, extension);
          break;
        default:
          NOTREACHED();
      }

      reload_actions_.erase(it);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const extensions::UnloadedExtensionInfo* unload_info =
          content::Details<extensions::UnloadedExtensionInfo>(details).ptr();
      if (!unload_info->extension->is_platform_app())
        break;

      if (unload_info->reason == extension_misc::UNLOAD_REASON_DISABLE) {
        ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
        if ((prefs->GetDisableReasons(unload_info->extension->id()) &
                Extension::DISABLE_RELOAD) &&
            !extensions::ShellWindowRegistry::Get(profile_)->
                GetShellWindowsForApp(unload_info->extension->id()).empty()) {
          reload_actions_[unload_info->extension->id()] = RELOAD_ACTION_LAUNCH;
        }
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace apps
