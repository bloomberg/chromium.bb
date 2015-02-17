// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
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
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
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

  ~ProgrammableInstallPrompt() override {}

  void ConfirmStandaloneInstall(
      Delegate* delegate,
      const Extension* extension,
      SkBitmap* icon,
      scoped_refptr<ExtensionInstallPrompt::Prompt> prompt) override {
    delegate_ = delegate;
  }

  static bool Ready() {
    return delegate_ != NULL;
  }

  static void Accept() {
    delegate_->InstallUIProceed();
  }

  static void Reject() {
    delegate_->InstallUIAbort(true);
  }

 private:
  static Delegate* delegate_;
};

ExtensionInstallPrompt::Delegate* ProgrammableInstallPrompt::delegate_;

// Fake inline installer which creates a programmable prompt in place of
// the normal dialog UI.
class WebstoreInlineInstallerForTest : public WebstoreInlineInstaller {
 public:
  WebstoreInlineInstallerForTest(WebContents* contents,
                                 const std::string& extension_id,
                                 const GURL& requestor_url,
                                 const Callback& callback)
      : WebstoreInlineInstaller(
            contents,
            kTestExtensionId,
            requestor_url,
            base::Bind(DummyCallback)),
        programmable_prompt_(NULL) {
  }

  scoped_ptr<ExtensionInstallPrompt> CreateInstallUI() override {
    programmable_prompt_ = new ProgrammableInstallPrompt(web_contents());
    return make_scoped_ptr(programmable_prompt_);
  }

 private:
  ~WebstoreInlineInstallerForTest() override {}

  friend class base::RefCountedThreadSafe<WebstoreStandaloneInstaller>;

  static void DummyCallback(bool success,
                            const std::string& error,
                            webstore_install::Result result) {
  }

  ProgrammableInstallPrompt* programmable_prompt_;
};

class WebstoreInlineInstallerForTestFactory :
    public WebstoreInlineInstallerFactory {
  ~WebstoreInlineInstallerForTestFactory() override {}
  WebstoreInlineInstaller* CreateInstaller(
      WebContents* contents,
      const std::string& webstore_item_id,
      const GURL& requestor_url,
      const WebstoreStandaloneInstaller::Callback& callback) override {
    return new WebstoreInlineInstallerForTest(
        contents, webstore_item_id, requestor_url, callback);
  }
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
                       ShouldBlockInlineInstallFromPopupWindow) {
  GURL install_url =
      GenerateTestServerUrl(kAppDomain, "install_from_popup.html");
  // Disable popup blocking for the test url.
  browser()->profile()->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURL(install_url),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_POPUPS,
      std::string(), CONTENT_SETTING_ALLOW);
  ui_test_utils::NavigateToURL(browser(), install_url);
  // The test page opens a popup which is a new |browser| window.
  Browser* popup_browser = chrome::FindLastActiveWithProfile(
      browser()->profile(), chrome::GetActiveDesktop());
  WebContents* popup_contents =
      popup_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(base::ASCIIToUTF16("POPUP"), popup_contents->GetTitle());
  RunTest(popup_contents, "runTest");
}

// Ensure that inline-installing a disabled extension simply re-enables it.
IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallerTest,
                       ReinstallDisabledExtension) {
  // Install an extension via inline install, and confirm it is successful.
  ExtensionInstallPrompt::g_auto_confirm_for_tests =
      ExtensionInstallPrompt::ACCEPT;
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
    ExtensionInstallPrompt::g_auto_confirm_for_tests =
        ExtensionInstallPrompt::ACCEPT;
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
