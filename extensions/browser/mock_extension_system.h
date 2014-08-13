// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MOCK_EXTENSIONS_SYSTEM_H_
#define EXTENSIONS_BROWSER_MOCK_EXTENSIONS_SYSTEM_H_

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
  virtual void InitForRegularProfile(bool extensions_enabled) OVERRIDE;
  virtual ExtensionService* extension_service() OVERRIDE;
  virtual RuntimeData* runtime_data() OVERRIDE;
  virtual ManagementPolicy* management_policy() OVERRIDE;
  virtual UserScriptMaster* user_script_master() OVERRIDE;
  virtual ProcessManager* process_manager() OVERRIDE;
  virtual StateStore* state_store() OVERRIDE;
  virtual StateStore* rules_store() OVERRIDE;
  virtual InfoMap* info_map() OVERRIDE;
  virtual LazyBackgroundTaskQueue* lazy_background_task_queue() OVERRIDE;
  virtual EventRouter* event_router() OVERRIDE;
  virtual ExtensionWarningService* warning_service() OVERRIDE;
  virtual Blacklist* blacklist() OVERRIDE;
  virtual ErrorConsole* error_console() OVERRIDE;
  virtual InstallVerifier* install_verifier() OVERRIDE;
  virtual QuotaService* quota_service() OVERRIDE;
  virtual const OneShotEvent& ready() const OVERRIDE;
  virtual ContentVerifier* content_verifier() OVERRIDE;
  virtual scoped_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) OVERRIDE;

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
      content::BrowserContext* context) const OVERRIDE {
    return new T(context);
  }

  // ExtensionSystemProvider overrides:
  virtual ExtensionSystem* GetForBrowserContext(
      content::BrowserContext* context) OVERRIDE {
    return static_cast<ExtensionSystem*>(
        GetServiceForBrowserContext(context, true));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockExtensionSystemFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MOCK_EXTENSIONS_SYSTEM_H_
