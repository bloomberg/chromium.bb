// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_system.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_prefs_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/value_store/testing_value_store.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/process_manager.h"

using content::BrowserThread;

namespace extensions {

TestExtensionSystem::TestExtensionSystem(Profile* profile)
    : profile_(profile),
      value_store_(NULL),
      info_map_(new ExtensionInfoMap()),
      error_console_(new ErrorConsole(profile, NULL)) {
}

TestExtensionSystem::~TestExtensionSystem() {
}

void TestExtensionSystem::Shutdown() {
  process_manager_.reset();
}

void TestExtensionSystem::CreateProcessManager() {
  process_manager_.reset(ProcessManager::Create(profile_));
}

void TestExtensionSystem::SetProcessManager(ProcessManager* manager) {
  process_manager_.reset(manager);
}

ExtensionPrefs* TestExtensionSystem::CreateExtensionPrefs(
    const CommandLine* command_line,
    const base::FilePath& install_directory) {
  bool extensions_disabled =
      command_line && command_line->HasSwitch(switches::kDisableExtensions);

  // Note that the GetPrefs() creates a TestingPrefService, therefore
  // the extension controlled pref values set in ExtensionPrefs
  // are not reflected in the pref service. One would need to
  // inject a new ExtensionPrefStore(extension_pref_value_map, false).

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Create(
      profile_->GetPrefs(),
      install_directory,
      ExtensionPrefValueMapFactory::GetForBrowserContext(profile_),
      extensions_disabled);
    ExtensionPrefsFactory::GetInstance()->SetInstanceForTesting(
        profile_,
        extension_prefs);
    return extension_prefs;
}

ExtensionService* TestExtensionSystem::CreateExtensionService(
    const CommandLine* command_line,
    const base::FilePath& install_directory,
    bool autoupdate_enabled) {
  if (!ExtensionPrefs::Get(profile_))
    CreateExtensionPrefs(command_line, install_directory);
  // The ownership of |value_store_| is immediately transferred to state_store_,
  // but we keep a naked pointer to the TestingValueStore.
  scoped_ptr<TestingValueStore> value_store(new TestingValueStore());
  value_store_ = value_store.get();
  state_store_.reset(
      new StateStore(profile_, value_store.PassAs<ValueStore>()));
  blacklist_.reset(new Blacklist(ExtensionPrefs::Get(profile_)));
  standard_management_policy_provider_.reset(
      new StandardManagementPolicyProvider(ExtensionPrefs::Get(profile_)));
  management_policy_.reset(new ManagementPolicy());
  management_policy_->RegisterProvider(
      standard_management_policy_provider_.get());
  extension_service_.reset(new ExtensionService(profile_,
                                                command_line,
                                                install_directory,
                                                ExtensionPrefs::Get(profile_),
                                                blacklist_.get(),
                                                autoupdate_enabled,
                                                true,
                                                &ready_));
  extension_service_->ClearProvidersForTesting();
  return extension_service_.get();
}

ExtensionService* TestExtensionSystem::extension_service() {
  return extension_service_.get();
}

ManagementPolicy* TestExtensionSystem::management_policy() {
  return management_policy_.get();
}

void TestExtensionSystem::SetExtensionService(ExtensionService* service) {
  extension_service_.reset(service);
}

UserScriptMaster* TestExtensionSystem::user_script_master() {
  return NULL;
}

ProcessManager* TestExtensionSystem::process_manager() {
  return process_manager_.get();
}

StateStore* TestExtensionSystem::state_store() {
  return state_store_.get();
}

StateStore* TestExtensionSystem::rules_store() {
  return state_store_.get();
}

ExtensionInfoMap* TestExtensionSystem::info_map() {
  return info_map_.get();
}

LazyBackgroundTaskQueue*
TestExtensionSystem::lazy_background_task_queue() {
  return NULL;
}

EventRouter* TestExtensionSystem::event_router() {
  return NULL;
}

ExtensionWarningService* TestExtensionSystem::warning_service() {
  return NULL;
}

Blacklist* TestExtensionSystem::blacklist() {
  return blacklist_.get();
}

const OneShotEvent& TestExtensionSystem::ready() const {
  return ready_;
}

ErrorConsole* TestExtensionSystem::error_console() {
  return error_console_.get();
}

// static
BrowserContextKeyedService* TestExtensionSystem::Build(
    content::BrowserContext* profile) {
  return new TestExtensionSystem(static_cast<Profile*>(profile));
}

}  // namespace extensions
