// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extension_loader.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/path_service.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_paths.h"

namespace extensions {

namespace {

class TestExtensionSystem : public MockExtensionSystem {
 public:
  explicit TestExtensionSystem(content::BrowserContext* context)
      : MockExtensionSystem(context),
        runtime_data_(ExtensionRegistry::Get(context)) {}
  ~TestExtensionSystem() override = default;

  RuntimeData* runtime_data() override { return &runtime_data_; }
  AppSorting* app_sorting() override { return &app_sorting_; }

 private:
  RuntimeData runtime_data_;
  NullAppSorting app_sorting_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionSystem);
};

}  // namespace

class ShellExtensionLoaderTest : public ExtensionsTest {
 protected:
  ShellExtensionLoaderTest()
      : ExtensionsTest(std::make_unique<content::TestBrowserThreadBundle>()) {}
  ~ShellExtensionLoaderTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();
    extensions_browser_client()->set_extension_system_factory(&factory_);
  }

  // Returns the path to a test directory.
  base::FilePath GetExtensionPath(const std::string& dir) {
    base::FilePath test_data_dir;
    PathService::Get(DIR_TEST_DATA, &test_data_dir);
    return test_data_dir.AppendASCII(dir);
  }

  // Verifies the extension is correctly created and enabled.
  void ExpectEnabled(const Extension* extension) {
    ASSERT_TRUE(extension);
    EXPECT_EQ(Manifest::COMMAND_LINE, extension->location());
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
    EXPECT_TRUE(registry->enabled_extensions().Contains(extension->id()));
    EXPECT_FALSE(registry->GetExtensionById(
        extension->id(),
        ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::ENABLED));
  }

  // Verifies the extension is correctly created but disabled.
  void ExpectDisabled(const Extension* extension) {
    ASSERT_TRUE(extension);
    EXPECT_EQ(Manifest::COMMAND_LINE, extension->location());
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
    EXPECT_TRUE(registry->disabled_extensions().Contains(extension->id()));
    EXPECT_FALSE(registry->GetExtensionById(
        extension->id(),
        ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::DISABLED));
  }

 private:
  MockExtensionSystemFactory<TestExtensionSystem> factory_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionLoaderTest);
};

// Tests loading an extension.
TEST_F(ShellExtensionLoaderTest, Extension) {
  ShellExtensionLoader loader(browser_context());

  const Extension* extension =
      loader.LoadExtension(GetExtensionPath("extension"));
  ExpectEnabled(extension);
}

// Tests with a non-existent directory.
TEST_F(ShellExtensionLoaderTest, NotFound) {
  ShellExtensionLoader loader(browser_context());

  const Extension* extension =
      loader.LoadExtension(GetExtensionPath("nonexistent"));
  ASSERT_FALSE(extension);
}

// Tests that the extension is added as enabled even if is disabled in
// ExtensionPrefs. Unlike Chrome, AppShell doesn't have a UI surface for
// re-enabling a disabled extension.
TEST_F(ShellExtensionLoaderTest, LoadAfterReloadFailure) {
  base::FilePath extension_path = GetExtensionPath("extension");
  const ExtensionId extension_id =
      crx_file::id_util::GenerateIdForPath(extension_path);
  ExtensionPrefs::Get(browser_context())
      ->SetExtensionDisabled(extension_id, disable_reason::DISABLE_RELOAD);

  ShellExtensionLoader loader(browser_context());
  const Extension* extension = loader.LoadExtension(extension_path);
  ExpectEnabled(extension);
}

// Tests that the extension is not added if it is disabled in ExtensionPrefs
// for reasons beyond reloading.
TEST_F(ShellExtensionLoaderTest, LoadDisabledExtension) {
  base::FilePath extension_path = GetExtensionPath("extension");
  const ExtensionId extension_id =
      crx_file::id_util::GenerateIdForPath(extension_path);
  ExtensionPrefs::Get(browser_context())
      ->SetExtensionDisabled(
          extension_id,
          disable_reason::DISABLE_RELOAD | disable_reason::DISABLE_USER_ACTION);

  ShellExtensionLoader loader(browser_context());
  const Extension* extension = loader.LoadExtension(extension_path);
  ExpectDisabled(extension);
}

}  // namespace extensions
