// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/test_browser_window.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/test_util.h"

namespace extensions {

class DeveloperPrivateApiUnitTest : public ExtensionServiceTestBase {
 protected:
  DeveloperPrivateApiUnitTest() {}
  ~DeveloperPrivateApiUnitTest() override {}

  // A wrapper around extension_function_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<UIThreadExtensionFunction>& function,
                   const base::ListValue& args);

  // Loads an unpacked extension that is backed by a real directory, allowing
  // it to be reloaded.
  const Extension* LoadUnpackedExtension();

  // Tests a developer private function (T) that sets an extension pref, and
  // verifies it with |has_pref|.
  template<typename T>
  void TestExtensionPrefSetting(
      bool (*has_pref)(const std::string&, content::BrowserContext*));

  Browser* browser() { return browser_.get(); }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  scoped_ptr<TestBrowserWindow> browser_window_;
  scoped_ptr<Browser> browser_;

  ScopedVector<TestExtensionDir> test_extension_dirs_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateApiUnitTest);
};

bool DeveloperPrivateApiUnitTest::RunFunction(
    const scoped_refptr<UIThreadExtensionFunction>& function,
    const base::ListValue& args) {
  return extension_function_test_utils::RunFunction(
      function.get(),
      make_scoped_ptr(args.DeepCopy()),
      browser(),
      extension_function_test_utils::NONE);
}

const Extension* DeveloperPrivateApiUnitTest::LoadUnpackedExtension() {
  const char kManifest[] =
      "{"
      " \"name\": \"foo\","
      " \"version\": \"1.0\","
      " \"manifest_version\": 2"
      "}";

  test_extension_dirs_.push_back(new TestExtensionDir);
  TestExtensionDir* dir = test_extension_dirs_.back();
  dir->WriteManifest(kManifest);

  // TODO(devlin): We should extract out methods to load an unpacked extension
  // synchronously. We do it in ExtensionBrowserTest, but that's not helpful
  // for unittests.
  TestExtensionRegistryObserver registry_observer(registry());
  scoped_refptr<UnpackedInstaller> installer(
      UnpackedInstaller::Create(service()));
  installer->Load(dir->unpacked_path());
  base::FilePath extension_path =
      base::MakeAbsoluteFilePath(dir->unpacked_path());
  const Extension* extension = nullptr;
  do {
    extension = registry_observer.WaitForExtensionLoaded();
  } while (extension->path() != extension_path);
  // The fact that unpacked extensions get file access by default is an
  // irrelevant detail to these tests. Disable it.
  ExtensionPrefs::Get(browser_context())->SetAllowFileAccess(extension->id(),
                                                             false);
  return extension;
}

template<typename T>
void DeveloperPrivateApiUnitTest::TestExtensionPrefSetting(
    bool (*has_pref)(const std::string&, content::BrowserContext*)) {
  // Sadly, we need a "real" directory here, because toggling incognito causes
  // a reload (which needs a path).
  std::string extension_id = LoadUnpackedExtension()->id();

  scoped_refptr<UIThreadExtensionFunction> function(new T());

  base::ListValue enable_args;
  enable_args.AppendString(extension_id);
  enable_args.AppendBoolean(true);

  EXPECT_FALSE(has_pref(extension_id, profile()));

  // Pref-setting should require a user action.
  EXPECT_FALSE(RunFunction(function, enable_args));
  EXPECT_EQ(std::string("This action requires a user gesture."),
            function->GetError());

  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  function = new T();
  EXPECT_TRUE(RunFunction(function, enable_args));
  EXPECT_TRUE(has_pref(extension_id, profile()));

  base::ListValue disable_args;
  disable_args.AppendString(extension_id);
  disable_args.AppendBoolean(false);
  function = new T();
  EXPECT_TRUE(RunFunction(function, disable_args));
  EXPECT_FALSE(has_pref(extension_id, profile()));
}

void DeveloperPrivateApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  browser_window_.reset(new TestBrowserWindow());
  Browser::CreateParams params(profile(), chrome::HOST_DESKTOP_TYPE_NATIVE);
  params.type = Browser::TYPE_TABBED;
  params.window = browser_window_.get();
  browser_.reset(new Browser(params));
}

void DeveloperPrivateApiUnitTest::TearDown() {
  test_extension_dirs_.clear();
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test developerPrivate.allowIncognito.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateAllowIncognito) {
  TestExtensionPrefSetting<api::DeveloperPrivateAllowIncognitoFunction>(
      &util::IsIncognitoEnabled);
}

// Test developerPrivate.reload.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateReload) {
  const Extension* extension = LoadUnpackedExtension();
  std::string extension_id = extension->id();
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateReloadFunction());
  base::ListValue reload_args;
  reload_args.AppendString(extension_id);

  TestExtensionRegistryObserver registry_observer(registry());
  EXPECT_TRUE(RunFunction(function, reload_args));
  const Extension* unloaded_extension =
      registry_observer.WaitForExtensionUnloaded();
  EXPECT_EQ(extension, unloaded_extension);
  const Extension* reloaded_extension =
      registry_observer.WaitForExtensionLoaded();
  EXPECT_EQ(extension_id, reloaded_extension->id());
}

// Test developerPrivate.allowFileAccess.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateAllowFileAccess) {
  TestExtensionPrefSetting<api::DeveloperPrivateAllowFileAccessFunction>(
      &util::AllowFileAccess);
}

}  // namespace extensions
