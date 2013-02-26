// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SYSTEM_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SYSTEM_H_

#include "chrome/browser/extensions/extension_system.h"

class CommandLine;

namespace base {
class Clock;
class FilePath;
class Time;
}

namespace extensions {

// Test ExtensionSystem, for use with TestingProfile.
class TestExtensionSystem : public ExtensionSystem {
 public:
  explicit TestExtensionSystem(Profile* profile);
  virtual ~TestExtensionSystem();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Creates an ExtensionService initialized with the testing profile and
  // returns it.
  ExtensionService* CreateExtensionService(const CommandLine* command_line,
                                           const base::FilePath& install_directory,
                                           bool autoupdate_enabled);

  // Creates an ExtensionProcessManager. If not invoked, the
  // ExtensionProcessManager is NULL.
  void CreateExtensionProcessManager();

  // Creates an AlarmManager. Will be NULL otherwise.
  void CreateAlarmManager(base::Clock* clock);

  void CreateSocketManager();

  virtual void InitForRegularProfile(bool extensions_enabled) OVERRIDE {}
  virtual void InitForOTRProfile() OVERRIDE {}
  void SetExtensionService(ExtensionService* service);
  virtual ExtensionService* extension_service() OVERRIDE;
  virtual ManagementPolicy* management_policy() OVERRIDE;
  virtual UserScriptMaster* user_script_master() OVERRIDE;
  virtual ExtensionProcessManager* process_manager() OVERRIDE;
  virtual AlarmManager* alarm_manager() OVERRIDE;
  virtual StateStore* state_store() OVERRIDE;
  virtual StateStore* rules_store() OVERRIDE;
  virtual ExtensionPrefs* extension_prefs() OVERRIDE;
  virtual ShellWindowGeometryCache* shell_window_geometry_cache() OVERRIDE;
  virtual ExtensionInfoMap* info_map() OVERRIDE;
  virtual LazyBackgroundTaskQueue* lazy_background_task_queue() OVERRIDE;
  virtual MessageService* message_service() OVERRIDE;
  virtual EventRouter* event_router() OVERRIDE;
  virtual RulesRegistryService* rules_registry_service() OVERRIDE;
  virtual ApiResourceManager<BluetoothSocketResource>*
      bluetooth_socket_resource_manager() OVERRIDE;
  virtual ApiResourceManager<SerialConnection>* serial_connection_manager()
      OVERRIDE;
  virtual ApiResourceManager<Socket>* socket_manager() OVERRIDE;
  virtual ApiResourceManager<UsbDeviceResource>* usb_device_resource_manager()
      OVERRIDE;
  virtual ExtensionWarningService* warning_service() OVERRIDE;
  virtual Blacklist* blacklist() OVERRIDE;

  // Factory method for tests to use with SetTestingProfile.
  static ProfileKeyedService* Build(Profile* profile);

 protected:
  Profile* profile_;

 private:
  // The Extension Preferences. Only created if CreateExtensionService is
  // invoked.
  scoped_ptr<ExtensionPrefs> extension_prefs_;
  scoped_ptr<StateStore> state_store_;
  scoped_ptr<ShellWindowGeometryCache> shell_window_geometry_cache_;
  scoped_ptr<Blacklist> blacklist_;
  scoped_ptr<StandardManagementPolicyProvider>
      standard_management_policy_provider_;
  scoped_ptr<ManagementPolicy> management_policy_;
  scoped_ptr<ExtensionService> extension_service_;
  scoped_ptr<ExtensionProcessManager> extension_process_manager_;
  scoped_ptr<AlarmManager> alarm_manager_;
  scoped_refptr<ExtensionInfoMap> info_map_;
  scoped_ptr<ApiResourceManager<Socket> > socket_manager_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SYSTEM_H_
