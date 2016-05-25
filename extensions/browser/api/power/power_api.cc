// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/power/power_api.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/power.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

const char kPowerSaveBlockerDescription[] = "extension";

content::PowerSaveBlocker::PowerSaveBlockerType LevelToPowerSaveBlockerType(
    api::power::Level level) {
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

base::LazyInstance<BrowserContextKeyedAPIFactory<PowerAPI>> g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool PowerRequestKeepAwakeFunction::RunSync() {
  std::unique_ptr<api::power::RequestKeepAwake::Params> params(
      api::power::RequestKeepAwake::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(params->level != api::power::LEVEL_NONE);
  PowerAPI::Get(browser_context())->AddRequest(extension_id(), params->level);
  return true;
}

bool PowerReleaseKeepAwakeFunction::RunSync() {
  PowerAPI::Get(browser_context())->RemoveRequest(extension_id());
  return true;
}

// static
PowerAPI* PowerAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<PowerAPI>::Get(context);
}

// static
BrowserContextKeyedAPIFactory<PowerAPI>* PowerAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void PowerAPI::AddRequest(const std::string& extension_id,
                          api::power::Level level) {
  extension_levels_[extension_id] = level;
  UpdatePowerSaveBlocker();
}

void PowerAPI::RemoveRequest(const std::string& extension_id) {
  extension_levels_.erase(extension_id);
  UpdatePowerSaveBlocker();
}

void PowerAPI::SetCreateBlockerFunctionForTesting(
    CreateBlockerFunction function) {
  create_blocker_function_ =
      !function.is_null() ? function
                          : base::Bind(&content::PowerSaveBlocker::Create);
}

void PowerAPI::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   UnloadedExtensionInfo::Reason reason) {
  RemoveRequest(extension->id());
  UpdatePowerSaveBlocker();
}

PowerAPI::PowerAPI(content::BrowserContext* context)
    : browser_context_(context),
      create_blocker_function_(base::Bind(&content::PowerSaveBlocker::Create)),
      current_level_(api::power::LEVEL_SYSTEM) {
  ExtensionRegistry::Get(browser_context_)->AddObserver(this);
}

PowerAPI::~PowerAPI() {
}

void PowerAPI::UpdatePowerSaveBlocker() {
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
    std::unique_ptr<content::PowerSaveBlocker> new_blocker(
        create_blocker_function_.Run(type,
                                     content::PowerSaveBlocker::kReasonOther,
                                     kPowerSaveBlockerDescription));
    power_save_blocker_.swap(new_blocker);
    current_level_ = new_level;
  }
}

void PowerAPI::Shutdown() {
  // Unregister here rather than in the d'tor; otherwise this call will recreate
  // the already-deleted ExtensionRegistry.
  ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);
  power_save_blocker_.reset();
}

}  // namespace extensions
