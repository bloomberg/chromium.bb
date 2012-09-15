// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/power/power_api_manager.h"

#include "chrome/browser/chromeos/power/power_state_override.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"

namespace extensions {
namespace power {

// static
PowerApiManager* PowerApiManager::GetInstance() {
  return Singleton<PowerApiManager>::get();
}

void PowerApiManager::AddExtensionLock(const std::string& extension_id) {
  extension_ids_set_.insert(extension_id);
  UpdatePowerSettings();
}

void PowerApiManager::RemoveExtensionLock(const std::string& extension_id) {
  extension_ids_set_.erase(extension_id);
  UpdatePowerSettings();
}

void PowerApiManager::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    RemoveExtensionLock(
        content::Details<extensions::UnloadedExtensionInfo>(details)->
            extension->id());
    UpdatePowerSettings();
  } else if (type == content::NOTIFICATION_APP_TERMINATING) {
    // If the Chrome app is terminating, ensure we release our power overrides.
    power_state_override_.reset(NULL);
  } else {
    NOTREACHED() << "Unexpected notification " << type;
  }
}

PowerApiManager::PowerApiManager()
    : power_state_override_(NULL) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                  content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  UpdatePowerSettings();
}

PowerApiManager::~PowerApiManager() {}

void PowerApiManager::UpdatePowerSettings() {
  // If we have a wake lock and don't have the power state overriden.
  if (extension_ids_set_.size() && !power_state_override_.get()) {
    power_state_override_.reset(new chromeos::PowerStateOverride());
  // else, if we don't have any wake locks and do have a power override.
  } else if (extension_ids_set_.empty() && power_state_override_.get()) {
    power_state_override_.reset(NULL);
  }
}

}  // namespace power
}  // namespace extensions
