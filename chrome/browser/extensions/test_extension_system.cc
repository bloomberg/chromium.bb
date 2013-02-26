// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_system.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/api/messaging/message_service.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/extensions/shell_window_geometry_cache.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/value_store/testing_value_store.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

TestExtensionSystem::TestExtensionSystem(Profile* profile)
    : profile_(profile),
      info_map_(new ExtensionInfoMap()) {
}

TestExtensionSystem::~TestExtensionSystem() {
}

void TestExtensionSystem::Shutdown() {
  extension_process_manager_.reset();
}

void TestExtensionSystem::CreateExtensionProcessManager() {
  extension_process_manager_.reset(ExtensionProcessManager::Create(profile_));
}

void TestExtensionSystem::CreateAlarmManager(base::Clock* clock) {
  alarm_manager_.reset(new AlarmManager(profile_, clock));
}

void TestExtensionSystem::CreateSocketManager() {
  // Note that we're intentionally creating the socket manager on the wrong
  // thread (not the IO thread). This is because we don't want to presume or
  // require that there be an IO thread in a lightweight test context. If we do
  // need thread-specific behavior someday, we'll probably need something like
  // CreateSocketManagerOnThreadForTesting(thread_id). But not today.
  BrowserThread::ID id;
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&id));
  socket_manager_.reset(new ApiResourceManager<Socket>(id));
}

ExtensionService* TestExtensionSystem::CreateExtensionService(
    const CommandLine* command_line,
    const base::FilePath& install_directory,
    bool autoupdate_enabled) {
  bool extensions_disabled =
      command_line && command_line->HasSwitch(switches::kDisableExtensions);

  // Note that the GetPrefs() creates a TestingPrefService, therefore
  // the extension controlled pref values set in extension_prefs_
  // are not reflected in the pref service. One would need to
  // inject a new ExtensionPrefStore(extension_pref_value_map, false).

  extension_prefs_ = ExtensionPrefs::Create(
      profile_->GetPrefs(),
      install_directory,
      ExtensionPrefValueMapFactory::GetForProfile(profile_),
      extensions_disabled);
  state_store_.reset(new StateStore(profile_, new TestingValueStore()));
  shell_window_geometry_cache_.reset(
      new ShellWindowGeometryCache(profile_, extension_prefs_.get()));
  blacklist_.reset(new Blacklist(extension_prefs_.get()));
  standard_management_policy_provider_.reset(
      new StandardManagementPolicyProvider(extension_prefs_.get()));
  management_policy_.reset(new ManagementPolicy());
  management_policy_->RegisterProvider(
      standard_management_policy_provider_.get());
  extension_service_.reset(new ExtensionService(profile_,
                                                command_line,
                                                install_directory,
                                                extension_prefs_.get(),
                                                blacklist_.get(),
                                                autoupdate_enabled,
                                                true));
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

ExtensionProcessManager* TestExtensionSystem::process_manager() {
  return extension_process_manager_.get();
}

AlarmManager* TestExtensionSystem::alarm_manager() {
  return alarm_manager_.get();
}

StateStore* TestExtensionSystem::state_store() {
  return state_store_.get();
}

StateStore* TestExtensionSystem::rules_store() {
  return state_store_.get();
}

ExtensionPrefs* TestExtensionSystem::extension_prefs() {
  return extension_prefs_.get();
}

ShellWindowGeometryCache* TestExtensionSystem::shell_window_geometry_cache() {
  return shell_window_geometry_cache_.get();
}

ExtensionInfoMap* TestExtensionSystem::info_map() {
  return info_map_.get();
}

LazyBackgroundTaskQueue*
TestExtensionSystem::lazy_background_task_queue() {
  return NULL;
}

MessageService* TestExtensionSystem::message_service() {
  return NULL;
}

EventRouter* TestExtensionSystem::event_router() {
  return NULL;
}

RulesRegistryService* TestExtensionSystem::rules_registry_service() {
  return NULL;
}

ApiResourceManager<BluetoothSocketResource>*
TestExtensionSystem::bluetooth_socket_resource_manager() {
  return NULL;
}

ApiResourceManager<SerialConnection>*
TestExtensionSystem::serial_connection_manager() {
  return NULL;
}

ApiResourceManager<Socket>*TestExtensionSystem::socket_manager() {
  return socket_manager_.get();
}

ApiResourceManager<UsbDeviceResource>*
TestExtensionSystem::usb_device_resource_manager() {
  return NULL;
}

ExtensionWarningService* TestExtensionSystem::warning_service() {
  return NULL;
}

Blacklist* TestExtensionSystem::blacklist() {
  return blacklist_.get();
}

// static
ProfileKeyedService* TestExtensionSystem::Build(Profile* profile) {
  return new TestExtensionSystem(profile);
}

}  // namespace extensions
