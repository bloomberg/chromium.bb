// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_system.h"

#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"


TestExtensionSystem::TestExtensionSystem(Profile* profile)
    : profile_(profile) {
}

TestExtensionSystem::~TestExtensionSystem() {
}

void TestExtensionSystem::Shutdown() {
  extension_process_manager_.reset();
}

void TestExtensionSystem::CreateExtensionProcessManager() {
  extension_process_manager_.reset(ExtensionProcessManager::Create(profile_));
}

void TestExtensionSystem::CreateAlarmManager() {
  alarm_manager_.reset(new extensions::AlarmManager(profile_));
}

ExtensionService* TestExtensionSystem::CreateExtensionService(
    const CommandLine* command_line,
    const FilePath& install_directory,
    bool autoupdate_enabled) {
  bool extensions_disabled =
      command_line && command_line->HasSwitch(switches::kDisableExtensions);

  // Note that the GetPrefs() creates a TestingPrefService, therefore
  // the extension controlled pref values set in extension_prefs_
  // are not reflected in the pref service. One would need to
  // inject a new ExtensionPrefStore(extension_pref_value_map, false).

  extension_prefs_.reset(new ExtensionPrefs(
      profile_->GetPrefs(),
      install_directory,
      ExtensionPrefValueMapFactory::GetForProfile(profile_)));
  extension_prefs_->Init(extensions_disabled);
  extension_service_.reset(new ExtensionService(profile_,
                                                command_line,
                                                install_directory,
                                                extension_prefs_.get(),
                                                autoupdate_enabled,
                                                true));
  return extension_service_.get();
}

extensions::ManagementPolicy* TestExtensionSystem::CreateManagementPolicy() {
  management_policy_.reset(new extensions::ManagementPolicy());
  DCHECK(extension_prefs_.get());
  management_policy_->RegisterProvider(extension_prefs_.get());

  return management_policy();
}

ExtensionService* TestExtensionSystem::extension_service() {
  return extension_service_.get();
}

extensions::ManagementPolicy* TestExtensionSystem::management_policy() {
  return management_policy_.get();
}

void TestExtensionSystem::SetExtensionService(ExtensionService* service) {
  extension_service_.reset(service);
}

UserScriptMaster* TestExtensionSystem::user_script_master() {
  return NULL;
}

ExtensionDevToolsManager* TestExtensionSystem::devtools_manager() {
  return NULL;
}

ExtensionProcessManager* TestExtensionSystem::process_manager() {
  return extension_process_manager_.get();
}

extensions::AlarmManager* TestExtensionSystem::alarm_manager() {
  return alarm_manager_.get();
}

ExtensionInfoMap* TestExtensionSystem::info_map() {
  return NULL;
}

extensions::LazyBackgroundTaskQueue*
TestExtensionSystem::lazy_background_task_queue() {
  return NULL;
}

ExtensionMessageService* TestExtensionSystem::message_service() {
  return NULL;
}

ExtensionEventRouter* TestExtensionSystem::event_router() {
  return NULL;
}

extensions::RulesRegistryService*
TestExtensionSystem::rules_registry_service() {
  return NULL;
}

// static
ProfileKeyedService* TestExtensionSystem::Build(Profile* profile) {
  return new TestExtensionSystem(profile);
}
