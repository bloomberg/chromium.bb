// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/power/power_api_manager.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

const char kPowerSaveBlockerReason[] = "extension";

content::PowerSaveBlocker::PowerSaveBlockerType
LevelToPowerSaveBlockerType(core_api::power::Level level) {
  switch (level) {
    case core_api::power::LEVEL_SYSTEM:
      return content::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension;
    case core_api::power::LEVEL_DISPLAY:  // fallthrough
    case core_api::power::LEVEL_NONE:
      return content::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep;
  }
  NOTREACHED() << "Unhandled level " << level;
  return content::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep;
}

base::LazyInstance<BrowserContextKeyedAPIFactory<PowerApiManager> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
PowerApiManager* PowerApiManager::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<PowerApiManager>::Get(context);
}

// static
BrowserContextKeyedAPIFactory<PowerApiManager>*
PowerApiManager::GetFactoryInstance() {
  return g_factory.Pointer();
}

void PowerApiManager::AddRequest(const std::string& extension_id,
                                 core_api::power::Level level) {
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

void PowerApiManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  RemoveRequest(extension->id());
  UpdatePowerSaveBlocker();
}

PowerApiManager::PowerApiManager(content::BrowserContext* context)
    : browser_context_(context),
      create_blocker_function_(base::Bind(&content::PowerSaveBlocker::Create)),
      current_level_(core_api::power::LEVEL_SYSTEM) {
  ExtensionRegistry::Get(browser_context_)->AddObserver(this);
}

PowerApiManager::~PowerApiManager() {}

void PowerApiManager::UpdatePowerSaveBlocker() {
  if (extension_levels_.empty()) {
    power_save_blocker_.reset();
    return;
  }

  core_api::power::Level new_level = core_api::power::LEVEL_SYSTEM;
  for (ExtensionLevelMap::const_iterator it = extension_levels_.begin();
       it != extension_levels_.end(); ++it) {
    if (it->second == core_api::power::LEVEL_DISPLAY)
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

void PowerApiManager::Shutdown() {
  // Unregister here rather than in the d'tor; otherwise this call will recreate
  // the already-deleted ExtensionRegistry.
  ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);
  power_save_blocker_.reset();
}

}  // namespace extensions
