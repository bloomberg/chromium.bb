// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/webstore_inline_installer.h"
#include "chrome/browser/extensions/webstore_inline_installer_factory.h"
#include "chrome/browser/extensions/webstore_installer_test.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/permissions/permission_set.h"
#include "url/gurl.h"

using content::WebContents;

namespace extensions {

namespace {

const char kWebstoreDomain[] = "cws.com";
const char kAppDomain[] = "app.com";
const char kNonAppDomain[] = "nonapp.com";
const char kTestExtensionId[] = "ecglahbcnmdpdciemllbhojghbkagdje";
const char kTestDataPath[] = "extensions/api_test/webstore_inline_install";
const char kCrxFilename[] = "extension.crx";

// A struct for letting us store the actual parameters that were passed to
// the install callback.
struct InstallResult {
  bool success = false;
  std::string error;
  webstore_install::Result result = webstore_install::RESULT_LAST;
};

}  // namespace

class WebstoreInlineInstallerTest : public WebstoreInstallerTest {
 public:
  WebstoreInlineInstallerTest()
      : WebstoreInstallerTest(
            kWebstoreDomain,
            kTestDataPath,
            kCrxFilename,
            kAppDomain,
            kNonAppDomain) {}
};

class ProgrammableInstallPrompt : public ExtensionInstallPrompt {
 public:
  explicit ProgrammableInstallPrompt(WebContents* contents)
      : ExtensionInstallPrompt(contents)
  {}

  ~ProgrammableInstallPrompt() override { g_done_callback = nullptr; }

  void ShowDialog(
      const ExtensionInstallPrompt::DoneCallback& done_callback,
      const Extension* extension,
      const SkBitmap* icon,
      scoped_ptr<ExtensionInstallPrompt::Prompt> prompt,
      scoped_ptr<const extensions::PermissionSet> custom_permissions,
      const ShowDialogCallback& show_dialog_callback) override {
    done_callback_ = done_callback;
    g_done_callback = &done_callback_;
  }

  static bool Ready() { return g_done_callback != nullptr; }

  static void Accept() {
    g_done_callback->Run(ExtensionInstallPrompt::Result::ACCEPTED);
  }

  static void Reject() {
    g_done_callback->Run(ExtensionInstallPrompt::Result::USER_CANCELED);
  }

 private:
  static ExtensionInstallPrompt::DoneCallback* g_done_callback;

  ExtensionInstallPrompt::DoneCallback done_callback_;

  DISALLOW_COPY_AND_ASSIGN(ProgrammableInstallPrompt);
};

ExtensionInstallPrompt::DoneCallback*
    ProgrammableInstallPrompt::g_done_callback = nullptr;

// Fake inline installer which creates a programmable prompt in place of
// the normal dialog UI.
class WebstoreInlineInstallerForTest : public WebstoreInlineInstaller {
 public:
  WebstoreInlineInstallerForTest(WebContents* contents,
                                 content::RenderFrameHost* host,
                                 const std::string& extension_id,
                                 const GURL& requestor_url,
                                 const Callback& callback)
      : WebstoreInlineInstaller(
            contents,
            host,
            kTestExtensionId,
            requestor_url,
            base::Bind(&WebstoreInlineInstallerForTest::InstallCallback,
                       base::Unretained(this))),
        install_result_target_(nullptr),
        programmable_prompt_(nullptr) {}

  scoped_ptr<ExtensionInstallPrompt> CreateInstallUI() override {
    programmable_prompt_ = new ProgrammableInstallPrompt(web_contents());
    return make_scoped_ptr(programmable_prompt_);
  }

  // Added here to make it public so that test cases can call it below.
  bool CheckRequestorAlive() const override {
    return WebstoreInlineInstaller::CheckRequestorAlive();
  }

  // Tests that care about the actual arguments to the install callback can use
  // this to receive a copy in |install_result_target|.
  void set_install_result_target(
      scoped_ptr<InstallResult>* install_result_target) {
    install_result_target_ = install_result_target;
  }

 private:
  ~WebstoreInlineInstallerForTest() override {}

  friend class base::RefCountedThreadSafe<WebstoreStandaloneInstaller>;

  void InstallCallback(bool success,
                       const std::string& error,
                       webstore_install::Result result) {
    if (install_result_target_) {
      install_result_target_->reset(new InstallResult);
      (*install_result_target_)->success = success;
      (*install_result_target_)->error = error;
      (*install_result_target_)->result = result;
    }
  }

  // This can be set by tests that want to get the actual install callback
  // arguments.
  scoped_ptr<InstallResult>* install_result_target_;

  ProgrammableInstallPrompt* programmable_prompt_;
};

class WebstoreInlineInstallerForTestFactory :
    public WebstoreInlineInstallerFactory {
 public:
  WebstoreInlineInstallerForTestFactory() : last_installer_(nullptr) {}
  ~WebstoreInlineInstallerForTestFactory() override {}

  WebstoreInlineInstallerForTest* last_installer() { return last_installer_; }

  WebstoreInlineInstaller* CreateInstaller(
      WebContents* contents,
      content::RenderFrameHost* host,
      const std::string& webstore_item_id,
      const GURL& requestor_url,
      const WebstoreStandaloneInstaller::Callback& callback) override {
    last_installer_ = new WebstoreInlineInstallerForTest(
        contents, host, webstore_item_id, requestor_url, callback);
    return last_installer_;
  }

 private:
  // The last installer that was created.
  WebstoreInlineInstallerForTest* last_installer_;
};

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallerTest,
                       CloseTabBeforeInstallConfirmation) {
  GURL install_url = GenerateTestServerUrl(kAppDomain, "install.html");
  ui_test_utils::NavigateToURL(browser(), install_url);
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents);
  tab_helper->SetWebstoreInlineInstallerFactoryForTests(
      new WebstoreInlineInstallerForTestFactory());
  RunTestAsync("runTest");
  while (!ProgrammableInstallPrompt::Ready())
    base::RunLoop().RunUntilIdle();
  web_contents->Close();
  ProgrammableInstallPrompt::Accept();
}

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallerTest,
                       NavigateBeforeInstallConfirmation) {
  GURL install_url = GenerateTestServerUrl(kAppDomain, "install.html");
  ui_test_utils::NavigateToURL(browser(), install_url);
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents);
  WebstoreInlineInstallerForTestFactory* factory =
      new WebstoreInlineInstallerForTestFactory();
  tab_helper->SetWebstoreInlineInstallerFactoryForTests(factory);
  RunTestAsync("runTest");
  while (!ProgrammableInstallPrompt::Ready())
    base::RunLoop().RunUntilIdle();
  GURL new_url = GenerateTestServerUrl(kNonAppDomain, "empty.html");
  web_contents->GetController().LoadURL(
      new_url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  ASSERT_NE(factory->last_installer(), nullptr);
  EXPECT_NE(factory->last_installer()->web_contents(), nullptr);
  EXPECT_FALSE(factory->last_installer()->CheckRequestorAlive());

  // Right now the way we handle navigations away from the frame that began the
  // inline install is to just declare the requestor to be dead, but not to
  // kill the prompt (that would be a better UX, but more complicated to
  // implement). If we ever do change things to kill the prompt in this case,
  // the following code can be removed (it verifies that clicking ok on the
  // dialog does not result in an install).
  scoped_ptr<InstallResult> install_result;
  factory->last_installer()->set_install_result_target(&install_result);
  ASSERT_TRUE(ProgrammableInstallPrompt::Ready());
  ProgrammableInstallPrompt::Accept();
  ASSERT_NE(install_result.get(), nullptr);
  EXPECT_EQ(install_result->success, false);
  EXPECT_EQ(install_result->result, webstore_install::ABORTED);
}

// Flaky: https://crbug.com/537526.
IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallerTest,
                       DISABLED_ShouldBlockInlineInstallFromPopupWindow) {
  GURL install_url =
      GenerateTestServerUrl(kAppDomain, "install_from_popup.html");
  // Disable popup blocking for the test url.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSetting(ContentSettingsPattern::FromURL(install_url),
                          ContentSettingsPattern::Wildcard(),
                          CONTENT_SETTINGS_TYPE_POPUPS,
                          std::string(),
                          CONTENT_SETTING_ALLOW);
  ui_test_utils::NavigateToURL(browser(), install_url);
  // The test page opens a popup which is a new |browser| window.
  Browser* popup_browser =
      chrome::FindLastActiveWithProfile(browser()->profile());
  WebContents* popup_contents =
      popup_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(base::ASCIIToUTF16("POPUP"), popup_contents->GetTitle());
  RunTest(popup_contents, "runTest");
}

// Ensure that inline-installing a disabled extension simply re-enables it.
IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallerTest,
                       ReinstallDisabledExtension) {
  // Install an extension via inline install, and confirm it is successful.
  AutoAcceptInstall();
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "install.html"));
  RunTest("runTest");
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry->enabled_extensions().GetByID(kTestExtensionId));

  // Disable the extension.
  ExtensionService* extension_service =
      ExtensionSystem::Get(browser()->profile())->extension_service();
  extension_service->DisableExtension(kTestExtensionId,
                                      Extension::DISABLE_USER_ACTION);
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kTestExtensionId));

  // Revisit the inline install site and reinstall the extension. It should
  // simply be re-enabled, rather than try to install again.
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "install.html"));
  RunTest("runTest");
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kTestExtensionId));
  // Since it was disabled by user action, the prompt should have just been the
  // inline install prompt.
  EXPECT_EQ(ExtensionInstallPrompt::INLINE_INSTALL_PROMPT,
            ExtensionInstallPrompt::g_last_prompt_type_for_tests);

  // Disable the extension due to a permissions increase.
  extension_service->DisableExtension(kTestExtensionId,
                                      Extension::DISABLE_PERMISSIONS_INCREASE);
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kTestExtensionId));
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "install.html"));
  RunTest("runTest");
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kTestExtensionId));
  // The displayed prompt should be for the permissions increase, versus a
  // normal inline install prompt.
  EXPECT_EQ(ExtensionInstallPrompt::RE_ENABLE_PROMPT,
            ExtensionInstallPrompt::g_last_prompt_type_for_tests);

  ExtensionInstallPrompt::g_last_prompt_type_for_tests =
      ExtensionInstallPrompt::UNSET_PROMPT_TYPE;
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "install.html"));
  RunTest("runTest");
  // If the extension was already enabled, we should still display an inline
  // install prompt (until we come up with something better).
  EXPECT_EQ(ExtensionInstallPrompt::INLINE_INSTALL_PROMPT,
            ExtensionInstallPrompt::g_last_prompt_type_for_tests);
}

class WebstoreInlineInstallerListenerTest : public WebstoreInlineInstallerTest {
 public:
  WebstoreInlineInstallerListenerTest() {}
  ~WebstoreInlineInstallerListenerTest() override {}

 protected:
  void RunTest(const std::string& file_name) {
    AutoAcceptInstall();
    ui_test_utils::NavigateToURL(browser(),
                                 GenerateTestServerUrl(kAppDomain, file_name));
    WebstoreInstallerTest::RunTest("runTest");
  }
};

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallerListenerTest,
                       InstallStageListenerTest) {
  RunTest("install_stage_listener.html");
}

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallerListenerTest,
                       DownloadProgressListenerTest) {
  RunTest("download_progress_listener.html");
}

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallerListenerTest, BothListenersTest) {
  RunTest("both_listeners.html");
}

}  // namespace extensions
