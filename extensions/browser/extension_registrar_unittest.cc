// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registrar.h"

#include <memory>

#include "base/location.h"
#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

namespace {

class TestExtensionSystem : public MockExtensionSystem {
 public:
  explicit TestExtensionSystem(content::BrowserContext* context)
      : MockExtensionSystem(context) {}

  ~TestExtensionSystem() override {}

  void RegisterExtensionWithRequestContexts(
      const Extension* extension,
      const base::Closure& callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestExtensionSystem);
};

}  // namespace

class ExtensionRegistrarTest : public ExtensionsTest {
 public:
  ExtensionRegistrarTest()
      : ExtensionsTest(std::make_unique<content::TestBrowserThreadBundle>()) {}
  ~ExtensionRegistrarTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();
    extensions_browser_client()->set_extension_system_factory(&factory_);
    extension_ = ExtensionBuilder("extension").Build();
  }

 protected:
  // Verifies that the extension is in the given set in the ExtensionRegistry
  // and not in other sets.
  void ExpectInSet(ExtensionRegistry::IncludeFlag set_id) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

    EXPECT_EQ(set_id == ExtensionRegistry::ENABLED,
              registry->enabled_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::DISABLED,
              registry->disabled_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::TERMINATED,
              registry->terminated_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::BLACKLISTED,
              registry->blacklisted_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::BLOCKED,
              registry->blocked_extensions().Contains(extension_->id()));
  }

  bool IsExtensionReady() {
    return ExtensionRegistry::Get(browser_context())
        ->ready_extensions()
        .Contains(extension_->id());
  }

  scoped_refptr<const Extension> extension() const { return extension_; }

 private:
  MockExtensionSystemFactory<TestExtensionSystem> factory_;
  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionRegistrarTest);
};

// Adds and removes an extension.
TEST_F(ExtensionRegistrarTest, EnableAndRemove) {
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context());
  ExtensionRegistrar registrar(browser_context());

  registrar.EnableExtension(extension());
  ExpectInSet(ExtensionRegistry::ENABLED);
  EXPECT_FALSE(IsExtensionReady());

  TestExtensionRegistryObserver observer(extension_registry);
  observer.WaitForExtensionReady();
  EXPECT_TRUE(IsExtensionReady());

  // Calling RemoveExtension removes its entry from the enabled list and then
  // removes the extension.
  registrar.RemoveExtension(extension()->id(),
                            UnloadedExtensionReason::UNINSTALL);
  ExpectInSet(ExtensionRegistry::NONE);
}

// Disables an extension before removing it.
TEST_F(ExtensionRegistrarTest, EnableDisableAndRemove) {
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context());
  ExtensionRegistrar registrar(browser_context());

  registrar.EnableExtension(extension());
  ExpectInSet(ExtensionRegistry::ENABLED);
  EXPECT_FALSE(IsExtensionReady());

  TestExtensionRegistryObserver observer(extension_registry);
  observer.WaitForExtensionReady();
  EXPECT_TRUE(IsExtensionReady());

  // Disabling the extension deactivates it.
  registrar.DisableExtension(extension());
  ExpectInSet(ExtensionRegistry::DISABLED);
  EXPECT_FALSE(IsExtensionReady());

  // We can still call RemoveExtension to remove its entry from the disabled
  // list and remove the extension.
  registrar.RemoveExtension(extension()->id(),
                            UnloadedExtensionReason::UNINSTALL);
  ExpectInSet(ExtensionRegistry::NONE);
  EXPECT_FALSE(IsExtensionReady());
}

// Enables a disabled extension.
TEST_F(ExtensionRegistrarTest, EnableDisabled) {
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context());
  ExtensionRegistrar registrar(browser_context());

  registrar.EnableExtension(extension());
  ExpectInSet(ExtensionRegistry::ENABLED);
  EXPECT_FALSE(IsExtensionReady());
  TestExtensionRegistryObserver(extension_registry).WaitForExtensionReady();

  registrar.DisableExtension(extension());
  ExpectInSet(ExtensionRegistry::DISABLED);
  EXPECT_FALSE(IsExtensionReady());

  registrar.EnableExtension(extension());
  ExpectInSet(ExtensionRegistry::ENABLED);
  TestExtensionRegistryObserver(extension_registry).WaitForExtensionReady();

  registrar.RemoveExtension(extension()->id(),
                            UnloadedExtensionReason::UNINSTALL);
  ExpectInSet(ExtensionRegistry::NONE);
}

}  // namespace extensions
