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
#include "chrome/common/extensions/api/developer_private.h"
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

  testing::AssertionResult TestPackExtensionFunction(
      const base::ListValue& args,
      api::developer_private::PackStatus expected_status,
      int expected_flags);

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

testing::AssertionResult DeveloperPrivateApiUnitTest::TestPackExtensionFunction(
    const base::ListValue& args,
    api::developer_private::PackStatus expected_status,
    int expected_flags) {
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivatePackDirectoryFunction());
  if (!RunFunction(function, args))
    return testing::AssertionFailure() << "Could not run function.";

  // Extract the result. We don't have to test this here, since it's verified as
  // part of the general extension api system.
  const base::Value* response_value = nullptr;
  CHECK(function->GetResultList()->Get(0u, &response_value));
  scoped_ptr<api::developer_private::PackDirectoryResponse> response =
      api::developer_private::PackDirectoryResponse::FromValue(*response_value);
  CHECK(response);

  if (response->status != expected_status) {
    return testing::AssertionFailure() << "Expected status: " <<
        expected_status << ", found status: " << response->status <<
        ", message: " << response->message;
  }

  if (response->override_flags != expected_flags) {
    return testing::AssertionFailure() << "Expected flags: " <<
        expected_flags << ", found flags: " << response->override_flags;
  }

  return testing::AssertionSuccess();
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

// Test developerPrivate.packDirectory.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivatePackFunction) {
  ResetThreadBundle(content::TestBrowserThreadBundle::DEFAULT);

  base::FilePath root_path = data_dir().AppendASCII("good_unpacked");
  base::FilePath crx_path = data_dir().AppendASCII("good_unpacked.crx");
  base::FilePath pem_path = data_dir().AppendASCII("good_unpacked.pem");

  // First, test a directory that should pack properly.
  base::ListValue pack_args;
  pack_args.AppendString(root_path.AsUTF8Unsafe());
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_SUCCESS, 0));

  // Should have created crx file and pem file.
  EXPECT_TRUE(base::PathExists(crx_path));
  EXPECT_TRUE(base::PathExists(pem_path));

  // Deliberately don't cleanup the files, and append the pem path.
  pack_args.AppendString(pem_path.AsUTF8Unsafe());

  // Try to pack again - we should get a warning abot overwriting the crx.
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args,
      api::developer_private::PACK_STATUS_WARNING,
      ExtensionCreator::kOverwriteCRX));

  // Try to pack again, with the overwrite flag; this should succeed.
  pack_args.AppendInteger(ExtensionCreator::kOverwriteCRX);
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_SUCCESS, 0));

  // Try to pack a final time when omitting (an existing) pem file. We should
  // get an error.
  base::DeleteFile(crx_path, false);
  EXPECT_TRUE(pack_args.Remove(1u, nullptr));  // Remove the pem key argument.
  EXPECT_TRUE(pack_args.Remove(1u, nullptr));  // Remove the flags argument.
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_ERROR, 0));

  base::DeleteFile(crx_path, false);
  base::DeleteFile(pem_path, false);
}

}  // namespace extensions
