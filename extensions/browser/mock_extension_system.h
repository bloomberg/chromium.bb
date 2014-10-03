// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MOCK_EXTENSION_SYSTEM_H_
#define EXTENSIONS_BROWSER_MOCK_EXTENSION_SYSTEM_H_

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/common/one_shot_event.h"

namespace extensions {

// An empty ExtensionSystem for testing. Tests that need only specific
// parts of ExtensionSystem should derive from this class and override
// functions as needed. To use this, use
// TestExtensionsBrowserClient::set_extension_system_factory
// with the MockExtensionSystemFactory below.
class MockExtensionSystem : public ExtensionSystem {
 public:
  explicit MockExtensionSystem(content::BrowserContext* context);
  virtual ~MockExtensionSystem();

  content::BrowserContext* browser_context() { return browser_context_; }

  // ExtensionSystem overrides:
  virtual void InitForRegularProfile(bool extensions_enabled) override;
  virtual ExtensionService* extension_service() override;
  virtual RuntimeData* runtime_data() override;
  virtual ManagementPolicy* management_policy() override;
  virtual SharedUserScriptMaster* shared_user_script_master() override;
  virtual ProcessManager* process_manager() override;
  virtual StateStore* state_store() override;
  virtual StateStore* rules_store() override;
  virtual InfoMap* info_map() override;
  virtual LazyBackgroundTaskQueue* lazy_background_task_queue() override;
  virtual EventRouter* event_router() override;
  virtual WarningService* warning_service() override;
  virtual Blacklist* blacklist() override;
  virtual ErrorConsole* error_console() override;
  virtual InstallVerifier* install_verifier() override;
  virtual QuotaService* quota_service() override;
  virtual const OneShotEvent& ready() const override;
  virtual ContentVerifier* content_verifier() override;
  virtual scoped_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;
  virtual DeclarativeUserScriptMaster*
      GetDeclarativeUserScriptMasterByExtension(
          const ExtensionId& extension_id) override;

 private:
  content::BrowserContext* browser_context_;
  OneShotEvent ready_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionSystem);
};

// A factory to create a MockExtensionSystem. Sample use:
//
// MockExtensionSystemFactory<MockExtensionSystemSubclass> factory;
// TestExtensionsBrowserClient::set_extension_system_factory(factory);
template <typename T>
class MockExtensionSystemFactory : public ExtensionSystemProvider {
 public:
  MockExtensionSystemFactory()
      : ExtensionSystemProvider(
            "MockExtensionSystem",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(ExtensionRegistryFactory::GetInstance());
  }

  virtual ~MockExtensionSystemFactory() {}

  // BrowserContextKeyedServiceFactory overrides:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new T(context);
  }

  // ExtensionSystemProvider overrides:
  virtual ExtensionSystem* GetForBrowserContext(
      content::BrowserContext* context) override {
    return static_cast<ExtensionSystem*>(
        GetServiceForBrowserContext(context, true));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockExtensionSystemFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MOCK_EXTENSION_SYSTEM_H_
