// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/webstore_inline_installer.h"
#include "chrome/browser/extensions/webstore_inline_installer_factory.h"
#include "chrome/browser/extensions/webstore_installer_test.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
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

  virtual ~ProgrammableInstallPrompt() {}

  virtual void ConfirmStandaloneInstall(
      Delegate* delegate,
      const Extension* extension,
      SkBitmap* icon,
      scoped_refptr<ExtensionInstallPrompt::Prompt> prompt) OVERRIDE {
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

  virtual scoped_ptr<ExtensionInstallPrompt> CreateInstallUI() OVERRIDE {
    programmable_prompt_ = new ProgrammableInstallPrompt(web_contents());
    return make_scoped_ptr(programmable_prompt_).
        PassAs<ExtensionInstallPrompt>();
  }

 private:
  virtual ~WebstoreInlineInstallerForTest() {}

  friend class base::RefCountedThreadSafe<WebstoreStandaloneInstaller>;

  static void DummyCallback(bool success,
                            const std::string& error,
                            webstore_install::Result result) {
  }

  ProgrammableInstallPrompt* programmable_prompt_;
};

class WebstoreInlineInstallerForTestFactory :
    public WebstoreInlineInstallerFactory {
  virtual ~WebstoreInlineInstallerForTestFactory() {}
  virtual WebstoreInlineInstaller* CreateInstaller(
      WebContents* contents,
      const std::string& webstore_item_id,
      const GURL& requestor_url,
      const WebstoreStandaloneInstaller::Callback& callback) OVERRIDE {
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

// Ensure that inline-installing a disabled extension simply re-enables it.
IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallerTest,
                       ReinstallDisabledExtension) {
  // Install an extension via inline install, and confirm it is successful.
  ExtensionInstallPrompt::g_auto_confirm_for_tests =
      ExtensionInstallPrompt::ACCEPT;
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "install.html"));
  RunTest("runTest");
  ExtensionService* extension_service =
      ExtensionSystem::Get(browser()->profile())->extension_service();
  ASSERT_TRUE(extension_service->GetExtensionById(kTestExtensionId, false));

  // Disable the extension.
  extension_service->DisableExtension(kTestExtensionId,
                                      Extension::DISABLE_USER_ACTION);
  ASSERT_FALSE(extension_service->IsExtensionEnabled(kTestExtensionId));

  // Revisit the inline install site and reinstall the extension. It should
  // simply be re-enabled, rather than try to install again.
  ui_test_utils::NavigateToURL(
      browser(), GenerateTestServerUrl(kAppDomain, "install.html"));
  RunTest("runTest");
  ASSERT_TRUE(extension_service->IsExtensionEnabled(kTestExtensionId));
}

class WebstoreInlineInstallerListenerTest : public WebstoreInlineInstallerTest {
 public:
  WebstoreInlineInstallerListenerTest() {}
  virtual ~WebstoreInlineInstallerListenerTest() {}

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
