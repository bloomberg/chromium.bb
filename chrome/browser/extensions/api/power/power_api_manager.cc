// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/power/power_api_manager.h"

#include "base/bind.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

namespace {

const char kPowerSaveBlockerReason[] = "extension";

content::PowerSaveBlocker::PowerSaveBlockerType
LevelToPowerSaveBlockerType(api::power::Level level) {
  switch (level) {
    case api::power::LEVEL_SYSTEM:
      return content::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension;
    case api::power::LEVEL_DISPLAY:  // fallthrough
    case api::power::LEVEL_NONE:
      return content::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep;
  }
  NOTREACHED() << "Unhandled level " << level;
  return content::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep;
}

}  // namespace

// static
PowerApiManager* PowerApiManager::GetInstance() {
  return Singleton<PowerApiManager>::get();
}

void PowerApiManager::AddRequest(const std::string& extension_id,
                                 api::power::Level level) {
  extension_levels_[extension_id] = level;
  UpdatePowerSaveBlocker();
}

void PowerApiManager::RemoveRequest(const std::string& extension_id) {
  extension_levels_.erase(extension_id);
  UpdatePowerSaveBlocker();
}

void PowerApiManager::SetCreateBlockerFunctionForTesting(
    CreateBlockerFunction function) {
  create_blocker_function_ = !function.is_null() ? function :
      base::Bind(&content::PowerSaveBlocker::Create);
}

void PowerApiManager::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      RemoveRequest(content::Details<extensions::UnloadedExtensionInfo>(
          details)->extension->id());
      UpdatePowerSaveBlocker();
      break;
    case chrome::NOTIFICATION_APP_TERMINATING:
      power_save_blocker_.reset();
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

PowerApiManager::PowerApiManager()
    : create_blocker_function_(base::Bind(&content::PowerSaveBlocker::Create)),
      current_level_(api::power::LEVEL_SYSTEM) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                  content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

PowerApiManager::~PowerApiManager() {}

void PowerApiManager::UpdatePowerSaveBlocker() {
  if (extension_levels_.empty()) {
    power_save_blocker_.reset();
    return;
  }

  api::power::Level new_level = api::power::LEVEL_SYSTEM;
  for (ExtensionLevelMap::const_iterator it = extension_levels_.begin();
       it != extension_levels_.end(); ++it) {
    if (it->second == api::power::LEVEL_DISPLAY)
      new_level = it->second;
  }

  // If the level changed and we need to create a new blocker, do a swap
  // to ensure that there isn't a brief period where power management is
  // unblocked.
  if (!power_save_blocker_ || new_level != current_level_) {
    content::PowerSaveBlocker::PowerSaveBlockerType type =
        LevelToPowerSaveBlockerType(new_level);
    scoped_ptr<content::PowerSaveBlocker> new_blocker(
        create_blocker_function_.Run(type, kPowerSaveBlockerReason));
    power_save_blocker_.swap(new_blocker);
    current_level_ = new_level;
  }
}

}  // namespace extensions
