// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/gpu_info.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/mock_host_resolver.h"
#include "ui/gl/gl_switches.h"

using content::GpuFeatureType;

namespace utils = extension_function_test_utils;

namespace extensions {

namespace {

class WebstoreInstallListener : public WebstoreInstaller::Delegate {
 public:
  WebstoreInstallListener()
      : received_failure_(false), received_success_(false), waiting_(false) {}

  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE {
    received_success_ = true;
    id_ = id;

    if (waiting_) {
      waiting_ = false;
      MessageLoopForUI::current()->Quit();
    }
  }

  virtual void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      WebstoreInstaller::FailureReason reason) OVERRIDE {
    received_failure_ = true;
    id_ = id;
    error_ = error;

    if (waiting_) {
      waiting_ = false;
      MessageLoopForUI::current()->Quit();
    }
  }

  void Wait() {
    if (received_success_ || received_failure_)
      return;

    waiting_ = true;
    content::RunMessageLoop();
  }
  bool received_success() const { return received_success_; }
  const std::string& id() const { return id_; }

 private:
  bool received_failure_;
  bool received_success_;
  bool waiting_;
  std::string id_;
  std::string error_;
};

}  // namespace

// A base class for tests below.
class ExtensionWebstorePrivateApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryURL,
        "http://www.example.com/files/extensions/api_test");
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryInstallAutoConfirmForTests, "accept");
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // Start up the test server and get us ready for calling the install
    // API functions.
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
    ASSERT_TRUE(test_server()->Start());
    ExtensionInstallUI::DisableFailureUIForTests();

    ASSERT_TRUE(tmp_.CreateUniqueTempDirUnderPath(test_data_dir_));
    ASSERT_TRUE(file_util::CreateDirectory(tmp_.path()));
    ASSERT_TRUE(file_util::CopyDirectory(
        test_data_dir_.AppendASCII("webstore_private"),
        tmp_.path(),
        true));
  }

 protected:
  // Returns a test server URL, but with host 'www.example.com' so it matches
  // the web store app's extent that we set up via command line flags.
  virtual GURL GetTestServerURL(const std::string& path) {
    std::string basename = tmp_.path().BaseName().MaybeAsASCII();
    GURL url = test_server()->GetURL(
        std::string("files/extensions/api_test/") +
        basename +
        "/webstore_private/" +
        path);

    // Replace the host with 'www.example.com' so it matches the web store
    // app's extent.
    GURL::Replacements replace_host;
    std::string host_str("www.example.com");
    replace_host.SetHostStr(host_str);

    return url.ReplaceComponents(replace_host);
  }

  // Navigates to |page| and runs the Extension API test there. Any downloads
  // of extensions will return the contents of |crx_file|.
  bool RunInstallTest(const std::string& page, const std::string& crx_file) {
    GURL crx_url = GetTestServerURL(crx_file);
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryUpdateURL, crx_url.spec());

    GURL page_url = GetTestServerURL(page);
    return RunPageTest(page_url.spec());
  }

  ExtensionService* service() {
    return browser()->profile()->GetExtensionService();
  }

  base::ScopedTempDir tmp_;
};

class ExtensionWebstorePrivateBundleTest
    : public ExtensionWebstorePrivateApiTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionWebstorePrivateApiTest::SetUpInProcessBrowserTestFixture();

    // The test server needs to have already started, so setup the switch here
    // rather than in SetUpCommandLine.
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryDownloadURL,
        GetTestServerURL("bundle/%s.crx").spec());
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    ExtensionWebstorePrivateApiTest::TearDownInProcessBrowserTestFixture();
    for (size_t i = 0; i < test_crx_.size(); ++i)
      ASSERT_TRUE(file_util::Delete(test_crx_[i], false));
  }

 protected:
  // Packs the |manifest| file into a CRX using |id|'s PEM key.
  void PackCRX(const std::string& id, const std::string& manifest) {
    // Move the extension to a temporary directory.
    base::ScopedTempDir tmp;
    ASSERT_TRUE(tmp.CreateUniqueTempDir());
    ASSERT_TRUE(file_util::CreateDirectory(tmp.path()));

    base::FilePath tmp_manifest = tmp.path().AppendASCII("manifest.json");
    base::FilePath data_path =
        test_data_dir_.AppendASCII("webstore_private/bundle");
    base::FilePath manifest_path = data_path.AppendASCII(manifest);

    ASSERT_TRUE(file_util::PathExists(manifest_path));
    ASSERT_TRUE(file_util::CopyFile(manifest_path, tmp_manifest));

    PackCRX(id, tmp.path());
  }

  // Packs the extension at |ext_path| using |id|'s PEM key.
  void PackCRX(const std::string& id, const base::FilePath& ext_path) {
    base::FilePath data_path =
        tmp_.path().AppendASCII("webstore_private/bundle");
    base::FilePath pem_path = data_path.AppendASCII(id + ".pem");
    base::FilePath crx_path = data_path.AppendASCII(id + ".crx");
    base::FilePath destination = PackExtensionWithOptions(
        ext_path, crx_path, pem_path, base::FilePath());

    ASSERT_FALSE(destination.empty());
    ASSERT_EQ(destination, crx_path);

    test_crx_.push_back(destination);
  }

  // Creates an invalid CRX.
  void PackInvalidCRX(const std::string& id) {
    base::FilePath contents = test_data_dir_
        .AppendASCII("webstore_private")
        .AppendASCII("install_bundle_invalid.html");
    base::FilePath crx_path = test_data_dir_
        .AppendASCII("webstore_private/bundle")
        .AppendASCII(id + ".crx");

    ASSERT_TRUE(file_util::CopyFile(contents, crx_path));

    test_crx_.push_back(crx_path);
  }

 private:
  std::vector<base::FilePath> test_crx_;
};

class ExtensionWebstoreGetWebGLStatusTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // In linux, we need to launch GPU process to decide if WebGL is allowed.
    // Run it on top of osmesa to avoid bot driver issues.
#if defined(OS_LINUX)
    CHECK(test_launcher_utils::OverrideGLImplementation(
        command_line, gfx::kGLImplementationOSMesaName)) <<
        "kUseGL must not be set multiple times!";
#endif
  }

 protected:
  void RunTest(bool webgl_allowed) {
    static const char kEmptyArgs[] = "[]";
    static const char kWebGLStatusAllowed[] = "webgl_allowed";
    static const char kWebGLStatusBlocked[] = "webgl_blocked";
    scoped_refptr<GetWebGLStatusFunction> function =
        new GetWebGLStatusFunction();
    scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
            function.get(), kEmptyArgs, browser()));
    ASSERT_TRUE(result);
    EXPECT_EQ(base::Value::TYPE_STRING, result->GetType());
    std::string webgl_status = "";
    EXPECT_TRUE(result->GetAsString(&webgl_status));
    EXPECT_STREQ(webgl_allowed ? kWebGLStatusAllowed : kWebGLStatusBlocked,
                 webgl_status.c_str());
  }
};

// Test cases for webstore origin frame blocking.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       FrameWebstorePageBlocked) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  string16 expected_title = UTF8ToUTF16("PASS: about:blank");
  string16 failure_title = UTF8ToUTF16("FAIL");
  content::TitleWatcher watcher(contents, expected_title);
  watcher.AlsoWaitForTitle(failure_title);
  GURL url = test_server()->GetURL(
      "files/extensions/api_test/webstore_private/noframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  string16 final_title = watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, final_title);
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       FrameErrorPageBlocked) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  string16 expected_title = UTF8ToUTF16("PASS: about:blank");
  string16 failure_title = UTF8ToUTF16("FAIL");
  content::TitleWatcher watcher(contents, expected_title);
  watcher.AlsoWaitForTitle(failure_title);
  GURL url = test_server()->GetURL(
      "files/extensions/api_test/webstore_private/noframe2.html");
  ui_test_utils::NavigateToURL(browser(), url);
  string16 final_title = watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, final_title);
}

// Test cases where the user accepts the install confirmation dialog.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallAccepted) {
  ASSERT_TRUE(RunInstallTest("accepted.html", "extension.crx"));
}

// Test having the default download directory missing.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, MissingDownloadDir) {
  // Set a non-existent directory as the download path.
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath missing_directory = temp_dir.Take();
  EXPECT_TRUE(file_util::Delete(missing_directory, true));
  WebstoreInstaller::SetDownloadDirectoryForTests(&missing_directory);

  // Now run the install test, which should succeed.
  ASSERT_TRUE(RunInstallTest("accepted.html", "extension.crx"));

  // Cleanup.
  if (file_util::DirectoryExists(missing_directory))
    EXPECT_TRUE(file_util::Delete(missing_directory, true));
}

// Tests passing a localized name.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallLocalized) {
  ASSERT_TRUE(RunInstallTest("localized.html", "localized_extension.crx"));
}

// Now test the case where the user cancels the confirmation dialog.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallCancelled) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "cancel");
  ASSERT_TRUE(RunInstallTest("cancelled.html", "extension.crx"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IncorrectManifest1) {
  ASSERT_TRUE(RunInstallTest("incorrect_manifest1.html", "extension.crx"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IncorrectManifest2) {
  ASSERT_TRUE(RunInstallTest("incorrect_manifest2.html", "extension.crx"));
}

// Disabled: http://crbug.com/174399
#if defined(OS_WIN) && defined(USE_AURA)
#define MAYBE_AppInstallBubble DISABLED_AppInstallBubble
#else
#define MAYBE_AppInstallBubble AppInstallBubble
#endif

// Tests that we can request an app installed bubble (instead of the default
// UI when an app is installed).
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       MAYBE_AppInstallBubble) {
  WebstoreInstallListener listener;
  WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("app_install_bubble.html", "app.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ("iladmdjkfniedhfhcfoefgojhgaiaccc", listener.id());
}

// Tests using the iconUrl parameter to the install function.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IconUrl) {
  ASSERT_TRUE(RunInstallTest("icon_url.html", "extension.crx"));
}

// Tests that the Approvals are properly created in beginInstall.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, BeginInstall) {
  std::string appId = "iladmdjkfniedhfhcfoefgojhgaiaccc";
  std::string extensionId = "enfkhcelefdadlmkffamgdlgplcionje";
  ASSERT_TRUE(RunInstallTest("begin_install.html", "extension.crx"));

  scoped_ptr<WebstoreInstaller::Approval> approval =
      WebstorePrivateApi::PopApprovalForTesting(browser()->profile(), appId);
  EXPECT_EQ(appId, approval->extension_id);
  EXPECT_TRUE(approval->use_app_installed_bubble);
  EXPECT_FALSE(approval->skip_post_install_ui);
  EXPECT_EQ(browser()->profile(), approval->profile);

  approval = WebstorePrivateApi::PopApprovalForTesting(
      browser()->profile(), extensionId);
  EXPECT_EQ(extensionId, approval->extension_id);
  EXPECT_FALSE(approval->use_app_installed_bubble);
  EXPECT_FALSE(approval->skip_post_install_ui);
  EXPECT_EQ(browser()->profile(), approval->profile);
}

// Tests that themes are installed without an install prompt.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallTheme) {
  WebstoreInstallListener listener;
  WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("theme.html", "../../../theme.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ("iamefpfkojoapidjnbafmgkgncegbkad", listener.id());
}

// Tests that an error is properly reported when an empty crx is returned.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, EmptyCrx) {
  ASSERT_TRUE(RunInstallTest("empty.html", "empty.crx"));
}

// Tests successfully installing a bundle of 2 apps and 2 extensions.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateBundleTest, InstallBundle) {
  extensions::BundleInstaller::SetAutoApproveForTesting(true);

  PackCRX("bmfoocgfinpmkmlbjhcbofejhkhlbchk", "extension1.json");
  PackCRX("pkapffpjmiilhlhbibjhamlmdhfneidj", "extension2.json");
  PackCRX("begfmnajjkbjdgmffnjaojchoncnmngg", "app1.json");
  PackCRX("mpneghmdnmaolkljkipbhaienajcflfe", "app2.json");

  ASSERT_TRUE(RunPageTest(GetTestServerURL("install_bundle.html").spec()));
}

#if defined(OS_WIN)
// Acting flakey in Windows. http://crbug.com/160219
#define MAYBE_InstallBundleIncognito DISABLED_InstallBundleIncognito
#else
#define MAYBE_InstallBundleIncognito InstallBundleIncognito
#endif

// Tests that bundles can be installed from incognito windows.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateBundleTest,
                       MAYBE_InstallBundleIncognito) {
  extensions::BundleInstaller::SetAutoApproveForTesting(true);

  PackCRX("bmfoocgfinpmkmlbjhcbofejhkhlbchk", "extension1.json");
  PackCRX("pkapffpjmiilhlhbibjhamlmdhfneidj", "extension2.json");
  PackCRX("begfmnajjkbjdgmffnjaojchoncnmngg", "app1.json");
  PackCRX("mpneghmdnmaolkljkipbhaienajcflfe", "app2.json");

  ASSERT_TRUE(RunPageTest(GetTestServerURL("install_bundle.html").spec(),
                          ExtensionApiTest::kFlagUseIncognito));
}

// Tests the user canceling the bundle install prompt.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateBundleTest,
                       InstallBundleCancel) {
  // We don't need to create the CRX files since we are aborting the install.
  extensions::BundleInstaller::SetAutoApproveForTesting(false);
  ASSERT_TRUE(RunPageTest(GetTestServerURL(
      "install_bundle_cancel.html").spec()));
}

// Tests partially installing a bundle (2 succeed, 1 fails due to an invalid
// CRX, and 1 fails due to the manifests not matching).
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateBundleTest,
                       InstallBundleInvalid) {
  extensions::BundleInstaller::SetAutoApproveForTesting(true);

  PackInvalidCRX("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  PackCRX("bmfoocgfinpmkmlbjhcbofejhkhlbchk", "extension1.json");
  PackCRX("begfmnajjkbjdgmffnjaojchoncnmngg", "app1.json");

  ASSERT_TRUE(RunPageTest(GetTestServerURL(
      "install_bundle_invalid.html").spec()));

  ASSERT_TRUE(service()->GetExtensionById(
      "begfmnajjkbjdgmffnjaojchoncnmngg", false));
  ASSERT_FALSE(service()->GetExtensionById(
      "pkapffpjmiilhlhbibjhamlmdhfneidj", true));
  ASSERT_FALSE(service()->GetExtensionById(
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", true));
  ASSERT_FALSE(service()->GetExtensionById(
      "bmfoocgfinpmkmlbjhcbofejhkhlbchk", true));
}

// Tests getWebGLStatus function when WebGL is allowed.
IN_PROC_BROWSER_TEST_F(ExtensionWebstoreGetWebGLStatusTest, Allowed) {
  bool webgl_allowed = true;
  RunTest(webgl_allowed);
}

// Tests getWebGLStatus function when WebGL is blacklisted.
IN_PROC_BROWSER_TEST_F(ExtensionWebstoreGetWebGLStatusTest, Blocked) {
  static const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  content::GPUInfo gpu_info;
  content::GpuDataManager::GetInstance()->InitializeForTesting(
      json_blacklist, gpu_info);
  GpuFeatureType type =
      content::GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  EXPECT_EQ((type & content::GPU_FEATURE_TYPE_WEBGL),
            content::GPU_FEATURE_TYPE_WEBGL);

  bool webgl_allowed = false;
  RunTest(webgl_allowed);
}

}  // namespace extensions
