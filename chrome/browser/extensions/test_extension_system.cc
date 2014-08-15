// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_system.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/value_store/testing_value_store.h"

using content::BrowserThread;

namespace extensions {

TestExtensionSystem::TestExtensionSystem(Profile* profile)
    : profile_(profile),
      value_store_(NULL),
      info_map_(new InfoMap()),
      error_console_(new ErrorConsole(profile)),
      quota_service_(new QuotaService()) {}

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
      ExtensionsBrowserClient::Get()->CreateAppSorting().Pass(),
      extensions_disabled,
      std::vector<ExtensionPrefsObserver*>());
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
  install_verifier_.reset(
      new InstallVerifier(ExtensionPrefs::Get(profile_), profile_));
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
  runtime_data_.reset(new RuntimeData(ExtensionRegistry::Get(profile_)));
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

RuntimeData* TestExtensionSystem::runtime_data() {
  return runtime_data_.get();
}

ManagementPolicy* TestExtensionSystem::management_policy() {
  return management_policy_.get();
}

void TestExtensionSystem::SetExtensionService(ExtensionService* service) {
  extension_service_.reset(service);
}

SharedUserScriptMaster* TestExtensionSystem::shared_user_script_master() {
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

InfoMap* TestExtensionSystem::info_map() { return info_map_.get(); }

LazyBackgroundTaskQueue*
TestExtensionSystem::lazy_background_task_queue() {
  return NULL;
}

void TestExtensionSystem::SetEventRouter(scoped_ptr<EventRouter> event_router) {
  event_router_.reset(event_router.release());
}

EventRouter* TestExtensionSystem::event_router() { return event_router_.get(); }

ExtensionWarningService* TestExtensionSystem::warning_service() {
  return NULL;
}

Blacklist* TestExtensionSystem::blacklist() {
  return blacklist_.get();
}

ErrorConsole* TestExtensionSystem::error_console() {
  return error_console_.get();
}

InstallVerifier* TestExtensionSystem::install_verifier() {
  return install_verifier_.get();
}

QuotaService* TestExtensionSystem::quota_service() {
  return quota_service_.get();
}

const OneShotEvent& TestExtensionSystem::ready() const {
  return ready_;
}

ContentVerifier* TestExtensionSystem::content_verifier() {
  return NULL;
}

scoped_ptr<ExtensionSet> TestExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return extension_service()->shared_module_service()->GetDependentExtensions(
      extension);
}

DeclarativeUserScriptMaster*
TestExtensionSystem::GetDeclarativeUserScriptMasterByExtension(
    const ExtensionId& extension_id) {
  return NULL;
}

// static
KeyedService* TestExtensionSystem::Build(content::BrowserContext* profile) {
  return new TestExtensionSystem(static_cast<Profile*>(profile));
}

}  // namespace extensions
