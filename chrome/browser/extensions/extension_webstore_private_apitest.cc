// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_webstore_private_api.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/mock_host_resolver.h"
#include "ui/gfx/gl/gl_switches.h"

using namespace extension_function_test_utils;

namespace {

class WebstoreInstallListener : public WebstoreInstaller::Delegate {
 public:
  WebstoreInstallListener()
      : received_failure_(false), received_success_(false), waiting_(false) {}

  void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  void OnExtensionInstallFailure(const std::string& id,
                                 const std::string& error) OVERRIDE;
  void Wait();

  bool received_failure() const { return received_failure_; }
  bool received_success() const { return received_success_; }
  const std::string& id() const { return id_; }
  const std::string& error() const { return error_; }

 private:
  bool received_failure_;
  bool received_success_;
  bool waiting_;
  std::string id_;
  std::string error_;
};

void WebstoreInstallListener::OnExtensionInstallSuccess(const std::string& id) {
  received_success_ = true;
  id_ = id;

  if (waiting_) {
    waiting_ = false;
    MessageLoopForUI::current()->Quit();
  }
}

void WebstoreInstallListener::OnExtensionInstallFailure(
    const std::string& id, const std::string& error) {
  received_failure_ = true;
  id_ = id;
  error_ = error;

  if (waiting_) {
    waiting_ = false;
    MessageLoopForUI::current()->Quit();
  }
}

void WebstoreInstallListener::Wait() {
  if (received_success_ || received_failure_)
    return;

  waiting_ = true;
  ui_test_utils::RunMessageLoop();
}

}  // namespace

// A base class for tests below.
class ExtensionWebstorePrivateApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryURL, "http://www.example.com");
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryInstallAutoConfirmForTests, "accept");
  }

  void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // Start up the test server and get us ready for calling the install
    // API functions.
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
    ASSERT_TRUE(test_server()->Start());
    ExtensionInstallUI::DisableFailureUIForTests();
  }

 protected:
  // Returns a test server URL, but with host 'www.example.com' so it matches
  // the web store app's extent that we set up via command line flags.
  GURL GetTestServerURL(const std::string& path) {
    GURL url = test_server()->GetURL(
        std::string("files/extensions/api_test/webstore_private/") + path);

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
};

class ExtensionWebstorePrivateBundleTest
    : public ExtensionWebstorePrivateApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionWebstorePrivateApiTest::SetUpInProcessBrowserTestFixture();

    // The test server needs to have already started, so setup the switch here
    // rather than in SetUpCommandLine.
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryDownloadURL,
        GetTestServerURL("bundle/%s.crx").spec());

    PackCRX("begfmnajjkbjdgmffnjaojchoncnmngg");
    PackCRX("bmfoocgfinpmkmlbjhcbofejhkhlbchk");
    PackCRX("mpneghmdnmaolkljkipbhaienajcflfe");
  }

  void TearDownInProcessBrowserTestFixture() OVERRIDE {
    ExtensionWebstorePrivateApiTest::TearDownInProcessBrowserTestFixture();
    for (size_t i = 0; i < test_crx_.size(); ++i)
      ASSERT_TRUE(file_util::Delete(test_crx_[i], false));
  }

 private:
  void PackCRX(const std::string& id) {
    FilePath data_path = test_data_dir_.AppendASCII("webstore_private/bundle");
    FilePath dir_path = data_path.AppendASCII(id);
    FilePath pem_path = data_path.AppendASCII(id + ".pem");
    FilePath crx_path = data_path.AppendASCII(id + ".crx");
    FilePath destination = PackExtensionWithOptions(
        dir_path, crx_path, pem_path, FilePath());

    ASSERT_FALSE(destination.empty());
    ASSERT_EQ(destination, crx_path);

    test_crx_.push_back(destination);
  }

  std::vector<FilePath> test_crx_;
};

class ExtensionWebstoreGetWebGLStatusTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
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
    scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
            new GetWebGLStatusFunction(), kEmptyArgs, browser()));
    EXPECT_EQ(base::Value::TYPE_STRING, result->GetType());
    StringValue* value = static_cast<StringValue*>(result.get());
    std::string webgl_status = "";
    EXPECT_TRUE(value && value->GetAsString(&webgl_status));
    EXPECT_STREQ(webgl_allowed ? kWebGLStatusAllowed : kWebGLStatusBlocked,
                 webgl_status.c_str());
  }
};

// Test cases where the user accepts the install confirmation dialog.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallAccepted) {
  ASSERT_TRUE(RunInstallTest("accepted.html", "extension.crx"));
}

// Test having the default download directory missing.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, MissingDownloadDir) {
  // Set a non-existent directory as the download path.
  ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath missing_directory = temp_dir.Take();
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

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       IncorrectManifest1) {
  WebstoreInstallListener listener;
  WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("incorrect_manifest1.html", "extension.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_failure());
  ASSERT_EQ("Manifest file is invalid.", listener.error());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       IncorrectManifest2) {
  WebstoreInstallListener listener;
  WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("incorrect_manifest2.html", "extension.crx"));
  listener.Wait();
  EXPECT_TRUE(listener.received_failure());
  ASSERT_EQ("Manifest file is invalid.", listener.error());
}

// Tests that we can request an app installed bubble (instead of the default
// UI when an app is installed).
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       AppInstallBubble) {
  WebstoreInstallListener listener;
  WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("app_install_bubble.html", "app.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ("iladmdjkfniedhfhcfoefgojhgaiaccc", listener.id());
}

// Tests using the iconUrl parameter to the install function.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       IconUrl) {
  ASSERT_TRUE(RunInstallTest("icon_url.html", "extension.crx"));
}

// Tests using silentlyInstall to install extensions.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateBundleTest, SilentlyInstall) {
  WebstorePrivateApi::SetTrustTestIDsForTesting(true);
  ASSERT_TRUE(RunPageTest(GetTestServerURL("silently_install.html").spec()));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("1.0"));
  GpuBlacklist* blacklist = new GpuBlacklist("1.0");

  ASSERT_TRUE(blacklist->LoadGpuBlacklist(
      json_blacklist, GpuBlacklist::kAllOs));
  GpuDataManager::GetInstance()->SetGpuBlacklist(blacklist);
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(
      flags.flags(), static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));

  bool webgl_allowed = false;
  RunTest(webgl_allowed);
}
