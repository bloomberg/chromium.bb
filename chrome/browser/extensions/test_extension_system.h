// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SYSTEM_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SYSTEM_H_

#include "base/memory/scoped_vector.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"

class Profile;
class TestingValueStore;

namespace base {
class CommandLine;
class FilePath;
class Time;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class DeclarativeUserScriptMaster;
class ExtensionPrefs;
class RuntimeData;
class SharedUserScriptMaster;
class StandardManagementPolicyProvider;

// Test ExtensionSystem, for use with TestingProfile.
class TestExtensionSystem : public ExtensionSystem {
 public:
  explicit TestExtensionSystem(Profile* profile);
  virtual ~TestExtensionSystem();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Creates an ExtensionPrefs with the testing profile and returns it.
  // Useful for tests that need to modify prefs before creating the
  // ExtensionService.
  ExtensionPrefs* CreateExtensionPrefs(const base::CommandLine* command_line,
                                       const base::FilePath& install_directory);

  // Creates an ExtensionService initialized with the testing profile and
  // returns it, and creates ExtensionPrefs if it hasn't been created yet.
  ExtensionService* CreateExtensionService(
      const base::CommandLine* command_line,
      const base::FilePath& install_directory,
      bool autoupdate_enabled);

  // Creates a ProcessManager. If not invoked, the ProcessManager is NULL.
  void CreateProcessManager();

  // Allows the ProcessManager to be overriden, for example by a stub
  // implementation. Takes ownership of |manager|.
  void SetProcessManager(ProcessManager* manager);

  void CreateSocketManager();

  virtual void InitForRegularProfile(bool extensions_enabled) OVERRIDE {}
  void SetExtensionService(ExtensionService* service);
  virtual ExtensionService* extension_service() OVERRIDE;
  virtual RuntimeData* runtime_data() OVERRIDE;
  virtual ManagementPolicy* management_policy() OVERRIDE;
  virtual SharedUserScriptMaster* shared_user_script_master() OVERRIDE;
  virtual ProcessManager* process_manager() OVERRIDE;
  virtual StateStore* state_store() OVERRIDE;
  virtual StateStore* rules_store() OVERRIDE;
  TestingValueStore* value_store() { return value_store_; }
  virtual InfoMap* info_map() OVERRIDE;
  virtual LazyBackgroundTaskQueue* lazy_background_task_queue() OVERRIDE;
  void SetEventRouter(scoped_ptr<EventRouter> event_router);
  virtual EventRouter* event_router() OVERRIDE;
  virtual WarningService* warning_service() OVERRIDE;
  virtual Blacklist* blacklist() OVERRIDE;
  virtual ErrorConsole* error_console() OVERRIDE;
  virtual InstallVerifier* install_verifier() OVERRIDE;
  virtual QuotaService* quota_service() OVERRIDE;
  virtual const OneShotEvent& ready() const OVERRIDE;
  virtual ContentVerifier* content_verifier() OVERRIDE;
  virtual scoped_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) OVERRIDE;
  virtual DeclarativeUserScriptMaster*
      GetDeclarativeUserScriptMasterByExtension(
          const ExtensionId& extension_id) OVERRIDE;

  // Note that you probably want to use base::RunLoop().RunUntilIdle() right
  // after this to run all the accumulated tasks.
  void SetReady() { ready_.Signal(); }

  // Factory method for tests to use with SetTestingProfile.
  static KeyedService* Build(content::BrowserContext* profile);

 protected:
  Profile* profile_;

 private:
  scoped_ptr<StateStore> state_store_;
  // A pointer to the TestingValueStore owned by |state_store_|.
  TestingValueStore* value_store_;
  ScopedVector<DeclarativeUserScriptMaster> declarative_user_script_masters_;
  scoped_ptr<Blacklist> blacklist_;
  scoped_ptr<ManagementPolicy> management_policy_;
  scoped_ptr<RuntimeData> runtime_data_;
  scoped_ptr<ExtensionService> extension_service_;
  scoped_ptr<ProcessManager> process_manager_;
  scoped_refptr<InfoMap> info_map_;
  scoped_ptr<EventRouter> event_router_;
  scoped_ptr<ErrorConsole> error_console_;
  scoped_ptr<InstallVerifier> install_verifier_;
  scoped_ptr<QuotaService> quota_service_;
  OneShotEvent ready_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SYSTEM_H_
