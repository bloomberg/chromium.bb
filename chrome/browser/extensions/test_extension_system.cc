// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_system.h"

#include <utility>

#include "base/command_line.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/chrome_app_sorting.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/management_policy.h"
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
      quota_service_(new QuotaService()),
      app_sorting_(new ChromeAppSorting(profile_)) {
}

TestExtensionSystem::~TestExtensionSystem() {
}

void TestExtensionSystem::Shutdown() {
  if (extension_service_)
    extension_service_->Shutdown();
}

ExtensionService* TestExtensionSystem::CreateExtensionService(
    const base::CommandLine* command_line,
    const base::FilePath& install_directory,
    bool autoupdate_enabled) {
  // The ownership of |value_store_| is immediately transferred to state_store_,
  // but we keep a naked pointer to the TestingValueStore.
  scoped_ptr<TestingValueStore> value_store(new TestingValueStore());
  value_store_ = value_store.get();
  state_store_.reset(new StateStore(profile_, std::move(value_store)));
  management_policy_.reset(new ManagementPolicy());
  management_policy_->RegisterProviders(
      ExtensionManagementFactory::GetForBrowserContext(profile_)
          ->GetProviders());
  runtime_data_.reset(new RuntimeData(ExtensionRegistry::Get(profile_)));
  extension_service_.reset(new ExtensionService(profile_,
                                                command_line,
                                                install_directory,
                                                ExtensionPrefs::Get(profile_),
                                                Blacklist::Get(profile_),
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

ServiceWorkerManager* TestExtensionSystem::service_worker_manager() {
  return nullptr;
}

SharedUserScriptMaster* TestExtensionSystem::shared_user_script_master() {
  return NULL;
}

StateStore* TestExtensionSystem::state_store() {
  return state_store_.get();
}

StateStore* TestExtensionSystem::rules_store() {
  return state_store_.get();
}

InfoMap* TestExtensionSystem::info_map() { return info_map_.get(); }

QuotaService* TestExtensionSystem::quota_service() {
  return quota_service_.get();
}

AppSorting* TestExtensionSystem::app_sorting() {
  return app_sorting_.get();
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

void TestExtensionSystem::InstallUpdate(const std::string& extension_id,
                                        const base::FilePath& temp_dir) {
  NOTREACHED();
}

// static
scoped_ptr<KeyedService> TestExtensionSystem::Build(
    content::BrowserContext* profile) {
  return make_scoped_ptr(
      new TestExtensionSystem(static_cast<Profile*>(profile)));
}

void TestExtensionSystem::RecreateAppSorting() {
  app_sorting_.reset(new ChromeAppSorting(profile_));
}

}  // namespace extensions
