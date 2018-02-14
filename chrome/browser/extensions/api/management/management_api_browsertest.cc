// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/test/extension_test_message_listener.h"

namespace keys = extension_management_api_constants;
namespace util = extension_function_test_utils;

namespace extensions {

class ExtensionManagementApiBrowserTest : public ExtensionBrowserTest {
 protected:
  bool CrashEnabledExtension(const std::string& extension_id) {
    ExtensionHost* background_host =
        ProcessManager::Get(browser()->profile())
            ->GetBackgroundHostForExtension(extension_id);
    if (!background_host)
      return false;
    content::CrashTab(background_host->host_contents());
    return true;
  }

 private:
  ScopedInstallVerifierBypassForTest install_verifier_bypass_;
};

// We test this here instead of in an ExtensionApiTest because normal extensions
// are not allowed to call the install function.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest, InstallEvent) {
  ExtensionTestMessageListener listener1("ready", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/install_event")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener listener2("got_event", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/management/enabled_extension")));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest, LaunchApp) {
  ExtensionTestMessageListener listener1("app_launched", false);
  ExtensionTestMessageListener listener2("got_expected_error", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/simple_extension")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/packaged_app")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_app")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest,
                       LaunchAppFromBackground) {
  ExtensionTestMessageListener listener1("success", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/packaged_app")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_app_from_background")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest,
                       SelfUninstall) {
  ExtensionTestMessageListener listener1("success", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/self_uninstall_helper")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/self_uninstall")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest,
                       SelfUninstallNoPermissions) {
  ExtensionTestMessageListener listener1("success", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/self_uninstall_helper")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/self_uninstall_noperm")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest,
                       GetSelfNoPermissions) {
  ExtensionTestMessageListener listener1("success", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/get_self")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest,
                       CreateAppShortcutConfirmDialog) {
  const Extension* app = InstallExtension(
      test_data_dir_.AppendASCII("api_test/management/packaged_app"), 1);
  ASSERT_TRUE(app);

  const std::string app_id = app->id();

  scoped_refptr<ManagementCreateAppShortcutFunction> create_shortcut_function(
      new ManagementCreateAppShortcutFunction());
  create_shortcut_function->set_user_gesture(true);
  ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(true);
  util::RunFunctionAndReturnSingleResult(
      create_shortcut_function.get(),
      base::StringPrintf("[\"%s\"]", app_id.c_str()),
      browser());

  create_shortcut_function = new ManagementCreateAppShortcutFunction();
  create_shortcut_function->set_user_gesture(true);
  ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(false);
  EXPECT_TRUE(base::MatchPattern(
      util::RunFunctionAndReturnError(
          create_shortcut_function.get(),
          base::StringPrintf("[\"%s\"]", app_id.c_str()),
          browser()),
      keys::kCreateShortcutCanceledError));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest,
                       GetAllIncludesTerminated) {
  // Load an extension with a background page, so that we know it has a process
  // running.
  ExtensionTestMessageListener listener("ready", false);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("management/install_event"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // The management API should list this extension.
  scoped_refptr<ManagementGetAllFunction> function =
      new ManagementGetAllFunction();
  std::unique_ptr<base::Value> result(
      util::RunFunctionAndReturnSingleResult(function.get(), "[]", browser()));
  base::ListValue* list;
  ASSERT_TRUE(result->GetAsList(&list));
  EXPECT_EQ(1U, list->GetSize());

  // And it should continue to do so even after it crashes.
  ASSERT_TRUE(CrashEnabledExtension(extension->id()));

  function = new ManagementGetAllFunction();
  result.reset(util::RunFunctionAndReturnSingleResult(
      function.get(), "[]", browser()));
  ASSERT_TRUE(result->GetAsList(&list));
  EXPECT_EQ(1U, list->GetSize());
}

class ExtensionManagementApiEscalationTest :
    public ExtensionManagementApiBrowserTest {
 protected:
  // The id of the permissions escalation test extension we use.
  static const char kId[];

  void SetUpOnMainThread() override {
    ExtensionManagementApiBrowserTest::SetUpOnMainThread();
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    base::FilePath pem_path = test_data_dir_.
        AppendASCII("permissions_increase").AppendASCII("permissions.pem");
    base::FilePath path_v1 = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v1"),
        scoped_temp_dir_.GetPath().AppendASCII("permissions1.crx"), pem_path,
        base::FilePath());
    base::FilePath path_v2 = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v2"),
        scoped_temp_dir_.GetPath().AppendASCII("permissions2.crx"), pem_path,
        base::FilePath());

    ExtensionService* service = ExtensionSystem::Get(browser()->profile())->
        extension_service();

    // Install low-permission version of the extension.
    ASSERT_TRUE(InstallExtension(path_v1, 1));
    EXPECT_TRUE(service->GetExtensionById(kId, false) != NULL);

    // Update to a high-permission version - it should get disabled.
    EXPECT_FALSE(UpdateExtension(kId, path_v2, -1));
    EXPECT_TRUE(service->GetExtensionById(kId, false) == NULL);
    EXPECT_TRUE(service->GetExtensionById(kId, true) != NULL);
    EXPECT_TRUE(ExtensionPrefs::Get(browser()->profile())
                    ->DidExtensionEscalatePermissions(kId));
  }

  void SetEnabled(bool enabled, bool user_gesture,
                  const std::string& expected_error) {
    scoped_refptr<ManagementSetEnabledFunction> function(
        new ManagementSetEnabledFunction);
    const char* const enabled_string = enabled ? "true" : "false";
    if (user_gesture)
      function->set_user_gesture(true);
    function->SetRenderFrameHost(browser()->tab_strip_model()->
        GetActiveWebContents()->GetMainFrame());
    bool response = util::RunFunction(
        function.get(), base::StringPrintf("[\"%s\", %s]", kId, enabled_string),
        browser(), api_test_utils::NONE);
    if (expected_error.empty()) {
      EXPECT_EQ(true, response);
    } else {
      EXPECT_TRUE(response == false);
      EXPECT_EQ(expected_error, function->GetError());
    }
  }


 private:
  base::ScopedTempDir scoped_temp_dir_;
};

const char ExtensionManagementApiEscalationTest::kId[] =
    "pgdpcfcocojkjfbgpiianjngphoopgmo";

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiEscalationTest,
                       DisabledReason) {
  scoped_refptr<ManagementGetFunction> function =
      new ManagementGetFunction();
  std::unique_ptr<base::Value> result(util::RunFunctionAndReturnSingleResult(
      function.get(), base::StringPrintf("[\"%s\"]", kId), browser()));
  ASSERT_TRUE(result.get() != NULL);
  ASSERT_TRUE(result->is_dict());
  base::DictionaryValue* dict =
      static_cast<base::DictionaryValue*>(result.get());
  std::string reason;
  EXPECT_TRUE(dict->GetStringASCII(keys::kDisabledReasonKey, &reason));
  EXPECT_EQ(reason, std::string(keys::kDisabledReasonPermissionsIncrease));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiEscalationTest,
                       SetEnabled) {
  // Expect an error about no gesture.
  SetEnabled(true, false, keys::kGestureNeededForEscalationError);

  {
    // Expect an error that user cancelled the dialog.
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    SetEnabled(true, true, keys::kUserDidNotReEnableError);
  }

  {
    // This should succeed when user accepts dialog. We must wait for the
    // process to connect *and* for the channel to finish initializing before
    // trying to crash it. (NOTIFICATION_RENDERER_PROCESS_CREATED does not wait
    // for the latter and can cause KillProcess to fail on Windows.)
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_CREATED,
        content::NotificationService::AllSources());
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    SetEnabled(true, true, std::string());
    observer.Wait();
  }

  {
    // Crash the extension. Mock a reload by disabling and then enabling. The
    // extension should be reloaded and enabled.
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ASSERT_TRUE(CrashEnabledExtension(kId));
    SetEnabled(false, true, std::string());
    SetEnabled(true, true, std::string());
    const Extension* extension = ExtensionSystem::Get(browser()->profile())
                                     ->extension_service()
                                     ->GetExtensionById(kId, false);
    EXPECT_TRUE(extension);
  }
}

}  // namespace extensions
