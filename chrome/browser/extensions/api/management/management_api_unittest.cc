// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/test_browser_window.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/test_util.h"

namespace extensions {

namespace {

scoped_ptr<KeyedService> BuildManagementApi(content::BrowserContext* context) {
  return make_scoped_ptr(new ManagementAPI(context));
}

scoped_ptr<KeyedService> BuildEventRouter(content::BrowserContext* profile) {
  return make_scoped_ptr(
      new extensions::EventRouter(profile, ExtensionPrefs::Get(profile)));
}

}  // namespace

namespace constants = extension_management_api_constants;

// TODO(devlin): Unittests are awesome. Test more with unittests and less with
// heavy api/browser tests.
class ManagementApiUnitTest : public ExtensionServiceTestBase {
 protected:
  ManagementApiUnitTest() {}
  ~ManagementApiUnitTest() override {}

  // A wrapper around extension_function_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<UIThreadExtensionFunction>& function,
                   const base::ListValue& args);

  Browser* browser() { return browser_.get(); }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  scoped_ptr<TestBrowserWindow> browser_window_;
  scoped_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(ManagementApiUnitTest);
};

bool ManagementApiUnitTest::RunFunction(
    const scoped_refptr<UIThreadExtensionFunction>& function,
    const base::ListValue& args) {
  return extension_function_test_utils::RunFunction(
      function.get(),
      make_scoped_ptr(args.DeepCopy()),
      browser(),
      extension_function_test_utils::NONE);
}

void ManagementApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();
  ManagementAPI::GetFactoryInstance()->SetTestingFactory(profile(),
                                                         &BuildManagementApi);

  EventRouterFactory::GetInstance()->SetTestingFactory(profile(),
                                                       &BuildEventRouter);

  browser_window_.reset(new TestBrowserWindow());
  Browser::CreateParams params(profile());
  params.type = Browser::TYPE_TABBED;
  params.window = browser_window_.get();
  browser_.reset(new Browser(params));
}

void ManagementApiUnitTest::TearDown() {
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test the basic parts of management.setEnabled.
TEST_F(ManagementApiUnitTest, ManagementSetEnabled) {
  scoped_refptr<const Extension> extension = test_util::CreateEmptyExtension();
  service()->AddExtension(extension.get());
  std::string extension_id = extension->id();
  scoped_refptr<ManagementSetEnabledFunction> function(
      new ManagementSetEnabledFunction());

  base::ListValue disable_args;
  disable_args.AppendString(extension_id);
  disable_args.AppendBoolean(false);

  // Test disabling an (enabled) extension.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
  EXPECT_TRUE(RunFunction(function, disable_args)) << function->GetError();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

  base::ListValue enable_args;
  enable_args.AppendString(extension_id);
  enable_args.AppendBoolean(true);

  // Test re-enabling it.
  function = new ManagementSetEnabledFunction();
  EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // Test that the enable function checks management policy, so that we can't
  // disable an extension that is required.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->RegisterProvider(&provider);

  function = new ManagementSetEnabledFunction();
  EXPECT_FALSE(RunFunction(function, disable_args));
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUserCantModifyError,
                                           extension_id),
            function->GetError());
  policy->UnregisterProvider(&provider);

  // TODO(devlin): We should also test enabling an extenion that has escalated
  // permissions, but that needs a web contents (which is a bit of a pain in a
  // unit test).
}

// Tests management.uninstall.
// http://crbug.com/527228 flaky
TEST_F(ManagementApiUnitTest, DISABLED_ManagementUninstall) {
  // We need to be on the UI thread for this.
  ResetThreadBundle(content::TestBrowserThreadBundle::DEFAULT);
  scoped_refptr<const Extension> extension = test_util::CreateEmptyExtension();
  service()->AddExtension(extension.get());
  std::string extension_id = extension->id();

  base::ListValue uninstall_args;
  uninstall_args.AppendString(extension->id());

  // Auto-accept any uninstalls.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);

    // Uninstall requires a user gesture, so this should fail.
    scoped_refptr<UIThreadExtensionFunction> function(
        new ManagementUninstallFunction());
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    EXPECT_EQ(std::string(constants::kGestureNeededForUninstallError),
              function->GetError());

    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    function = new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
  }

  // Install the extension again, and try uninstalling, auto-canceling the
  // dialog.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    service()->AddExtension(extension.get());
    scoped_refptr<UIThreadExtensionFunction> function =
        new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // The uninstall should have failed.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());

    // Try again, using showConfirmDialog: false.
    scoped_ptr<base::DictionaryValue> options(new base::DictionaryValue());
    options->SetBoolean("showConfirmDialog", false);
    uninstall_args.Append(options.release());
    function = new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // This should still fail, since extensions can only suppress the dialog for
    // uninstalling themselves.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());

    // If we try uninstall the extension itself, the uninstall should succeed
    // (even though we auto-cancel any dialog), because the dialog is never
    // shown.
    uninstall_args.Remove(0u, nullptr);
    function = new ManagementUninstallSelfFunction();
    function->set_extension(extension);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
  }
}

}  // namespace extensions
