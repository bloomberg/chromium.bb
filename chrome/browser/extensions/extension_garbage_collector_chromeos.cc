// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_garbage_collector_chromeos.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

bool ExtensionGarbageCollectorChromeOS::shared_extensions_garbage_collected_ =
    false;

ExtensionGarbageCollectorChromeOS::ExtensionGarbageCollectorChromeOS(
    content::BrowserContext* context)
    : ExtensionGarbageCollector(context),
      disable_garbage_collection_(false) {
}

ExtensionGarbageCollectorChromeOS::~ExtensionGarbageCollectorChromeOS() {}

// static
ExtensionGarbageCollectorChromeOS* ExtensionGarbageCollectorChromeOS::Get(
    content::BrowserContext* context) {
  return static_cast<ExtensionGarbageCollectorChromeOS*>(
      ExtensionGarbageCollector::Get(context));
}

// static
void ExtensionGarbageCollectorChromeOS::ClearGarbageCollectedForTesting() {
  shared_extensions_garbage_collected_ = false;
}

void ExtensionGarbageCollectorChromeOS::GarbageCollectExtensions() {
  if (disable_garbage_collection_)
    return;

  // Process per-profile extensions dir.
  ExtensionGarbageCollector::GarbageCollectExtensions();

  if (!shared_extensions_garbage_collected_ &&
      CanGarbageCollectSharedExtensions()) {
    GarbageCollectSharedExtensions();
    shared_extensions_garbage_collected_ = true;
  }
}

bool ExtensionGarbageCollectorChromeOS::CanGarbageCollectSharedExtensions() {
  chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  if (!user_manager) {
    NOTREACHED();
    return false;
  }

  const user_manager::UserList& active_users = user_manager->GetLoggedInUsers();
  for (size_t i = 0; i < active_users.size(); i++) {
    Profile* profile =
        chromeos::ProfileHelper::Get()->GetProfileByUser(active_users[i]);
    ExtensionGarbageCollectorChromeOS* gc =
        ExtensionGarbageCollectorChromeOS::Get(profile);
    if (gc && gc->crx_installs_in_progress_ > 0)
      return false;
  }

  return true;
}

void ExtensionGarbageCollectorChromeOS::GarbageCollectSharedExtensions() {
  std::multimap<std::string, base::FilePath> paths;
  if (ExtensionAssetsManagerChromeOS::CleanUpSharedExtensions(&paths)) {
    ExtensionService* service =
        ExtensionSystem::Get(context_)->extension_service();
    if (!service->GetFileTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&GarbageCollectExtensionsOnFileThread,
                       ExtensionAssetsManagerChromeOS::GetSharedInstallDir(),
                       paths))) {
      NOTREACHED();
    }
  }
}

}  // namespace extensions
