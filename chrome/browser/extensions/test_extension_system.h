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
class DeclarativeUserScriptManager;
class ExtensionPrefs;
class RuntimeData;
class SharedUserScriptMaster;
class StandardManagementPolicyProvider;

// Test ExtensionSystem, for use with TestingProfile.
class TestExtensionSystem : public ExtensionSystem {
 public:
  explicit TestExtensionSystem(Profile* profile);
  ~TestExtensionSystem() override;

  // KeyedService implementation.
  void Shutdown() override;

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

  void CreateSocketManager();

  // Creates a LazyBackgroundTaskQueue. If not invoked, the
  // LazyBackgroundTaskQueue is NULL.
  void CreateLazyBackgroundTaskQueue();

  void InitForRegularProfile(bool extensions_enabled) override {}
  void SetExtensionService(ExtensionService* service);
  ExtensionService* extension_service() override;
  RuntimeData* runtime_data() override;
  ManagementPolicy* management_policy() override;
  SharedUserScriptMaster* shared_user_script_master() override;
  DeclarativeUserScriptManager* declarative_user_script_manager() override;
  StateStore* state_store() override;
  StateStore* rules_store() override;
  TestingValueStore* value_store() { return value_store_; }
  InfoMap* info_map() override;
  LazyBackgroundTaskQueue* lazy_background_task_queue() override;
  void SetEventRouter(scoped_ptr<EventRouter> event_router);
  EventRouter* event_router() override;
  ErrorConsole* error_console() override;
  InstallVerifier* install_verifier() override;
  QuotaService* quota_service() override;
  const OneShotEvent& ready() const override;
  ContentVerifier* content_verifier() override;
  scoped_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;

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
  scoped_ptr<DeclarativeUserScriptManager> declarative_user_script_manager_;
  scoped_ptr<ManagementPolicy> management_policy_;
  scoped_ptr<RuntimeData> runtime_data_;
  scoped_ptr<ExtensionService> extension_service_;
  scoped_refptr<InfoMap> info_map_;
  scoped_ptr<LazyBackgroundTaskQueue> lazy_background_task_queue_;
  scoped_ptr<EventRouter> event_router_;
  scoped_ptr<ErrorConsole> error_console_;
  scoped_ptr<InstallVerifier> install_verifier_;
  scoped_ptr<QuotaService> quota_service_;
  OneShotEvent ready_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SYSTEM_H_
