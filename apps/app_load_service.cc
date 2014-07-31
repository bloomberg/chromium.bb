// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_load_service.h"

#include "apps/app_load_service_factory.h"
#include "apps/app_restore_service.h"
#include "apps/app_window_registry.h"
#include "apps/launcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionSystem;

namespace apps {

AppLoadService::PostReloadAction::PostReloadAction()
    : action_type(LAUNCH),
      command_line(CommandLine::NO_PROGRAM) {
}

AppLoadService::AppLoadService(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::NotificationService::AllSources());
}

AppLoadService::~AppLoadService() {}

void AppLoadService::RestartApplication(const std::string& extension_id) {
  post_reload_actions_[extension_id].action_type = RESTART;
  ExtensionService* service = extensions::ExtensionSystem::Get(profile_)->
      extension_service();
  DCHECK(service);
  service->ReloadExtension(extension_id);
}

void AppLoadService::RestartApplicationIfRunning(
    const std::string& extension_id) {
  if (apps::AppRestoreService::Get(profile_)->IsAppRestorable(extension_id))
    RestartApplication(extension_id);
}

bool AppLoadService::LoadAndLaunch(const base::FilePath& extension_path,
                                   const CommandLine& command_line,
                                   const base::FilePath& current_dir) {
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile_)->extension_service();
  std::string extension_id;
  if (!extensions::UnpackedInstaller::Create(extension_service)->
          LoadFromCommandLine(base::FilePath(extension_path), &extension_id)) {
    return false;
  }

  // Schedule the app to be launched once loaded.
  PostReloadAction& action = post_reload_actions_[extension_id];
  action.action_type = LAUNCH_WITH_COMMAND_LINE;
  action.command_line = command_line;
  action.current_dir = current_dir;
  return true;
}

// static
AppLoadService* AppLoadService::Get(Profile* profile) {
  return apps::AppLoadServiceFactory::GetForProfile(profile);
}

void AppLoadService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING: {
      extensions::ExtensionHost* host =
          content::Details<extensions::ExtensionHost>(details).ptr();
      const Extension* extension = host->extension();
      // It is possible for an extension to be unloaded before it stops loading.
      if (!extension)
        break;
      std::map<std::string, PostReloadAction>::iterator it =
          post_reload_actions_.find(extension->id());
      if (it == post_reload_actions_.end())
        break;

      switch (it->second.action_type) {
        case LAUNCH:
          LaunchPlatformApp(profile_, extension);
          break;
        case RESTART:
          RestartPlatformApp(profile_, extension);
          break;
        case LAUNCH_WITH_COMMAND_LINE:
          LaunchPlatformAppWithCommandLine(
              profile_, extension, it->second.command_line,
              it->second.current_dir);
          break;
        default:
          NOTREACHED();
      }

      post_reload_actions_.erase(it);
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED: {
      const extensions::UnloadedExtensionInfo* unload_info =
          content::Details<extensions::UnloadedExtensionInfo>(details).ptr();
      if (!unload_info->extension->is_platform_app())
        break;

      extensions::ExtensionPrefs* extension_prefs =
          extensions::ExtensionPrefs::Get(profile_);
      if (WasUnloadedForReload(*unload_info) &&
          extension_prefs->IsActive(unload_info->extension->id()) &&
          !HasPostReloadAction(unload_info->extension->id())) {
        post_reload_actions_[unload_info->extension->id()].action_type = LAUNCH;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

bool AppLoadService::WasUnloadedForReload(
    const extensions::UnloadedExtensionInfo& unload_info) {
  if (unload_info.reason == extensions::UnloadedExtensionInfo::REASON_DISABLE) {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
     return (prefs->GetDisableReasons(unload_info.extension->id()) &
        Extension::DISABLE_RELOAD) != 0;
  }
  return false;
}

bool AppLoadService::HasPostReloadAction(const std::string& extension_id) {
  return post_reload_actions_.find(extension_id) != post_reload_actions_.end();
}

}  // namespace apps
