// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_installer.h"

#include <jni.h>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk.pb.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace {

const base::FilePath::CharType kTestDataDir[] =
    FILE_PATH_LITERAL("chrome/test/data");

// URL of mock WebAPK server.
const char* kServerUrl = "/webapkserver/";

// The best primary icon URL from Web Manifest. We use a random file in the test
// data directory. Since WebApkInstaller does not try to decode the file as an
// image it is OK that the file is not an image.
const char* kBestPrimaryIconUrl = "/simple.html";

// URL of file to download from the WebAPK server. We use a random file in the
// test data directory.
const char* kDownloadUrl = "/simple.html";

// The package name of the downloaded WebAPK.
const char* kDownloadedWebApkPackageName = "party.unicode";

// WebApkInstaller subclass where
// WebApkInstaller::StartInstallingDownloadedWebApk() and
// WebApkInstaller::StartUpdateUsingDownloadedWebApk() and
// WebApkInstaller::CanUseGooglePlayInstallService() and
// WebApkInstaller::InstallOrUpdateWebApkFromGooglePlay() are stubbed out.
class TestWebApkInstaller : public WebApkInstaller {
 public:
  TestWebApkInstaller(content::BrowserContext* browser_context,
                      const ShortcutInfo& shortcut_info,
                      const SkBitmap& shortcut_icon,
                      bool can_use_google_play_install_service)
      : WebApkInstaller(browser_context, shortcut_info, shortcut_icon),
        can_use_google_play_install_service_(
            can_use_google_play_install_service) {}

  void InstallDownloadedWebApk(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jstring>& file_path,
      const base::android::ScopedJavaLocalRef<jstring>& package_name) override {
    PostTaskToRunSuccessCallback();
  }

  void UpdateUsingDownloadedWebApk(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jstring>& file_path) override {
    PostTaskToRunSuccessCallback();
  }

  bool CanUseGooglePlayInstallService() override {
    return can_use_google_play_install_service_;
  }

  void InstallOrUpdateWebApkFromGooglePlay(const std::string& package_name,
                                           int version,
                                           const std::string& token) override {
    PostTaskToRunSuccessCallback();
  }

  void PostTaskToRunSuccessCallback() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&TestWebApkInstaller::OnResult, base::Unretained(this),
                   WebApkInstallResult::SUCCESS));
  }

 private:
  // Whether the Google Play Services can be used and the install delegate is
  // available.
  bool can_use_google_play_install_service_;

  DISALLOW_COPY_AND_ASSIGN(TestWebApkInstaller);
};

// Runs the WebApkInstaller installation process/update and blocks till done.
class WebApkInstallerRunner {
 public:
  WebApkInstallerRunner(content::BrowserContext* browser_context,
                        const GURL& best_primary_icon_url)
      : browser_context_(browser_context),
        best_primary_icon_url_(best_primary_icon_url),
        can_use_google_play_install_service_(false) {}

  ~WebApkInstallerRunner() {}

  void SetCanUseGooglePlayInstallService(
      bool can_use_google_play_install_service) {
    can_use_google_play_install_service_ = can_use_google_play_install_service;
  }

  void RunInstallWebApk() {
    WebApkInstaller::InstallAsyncForTesting(
        CreateWebApkInstaller(), base::Bind(&WebApkInstallerRunner::OnCompleted,
                                            base::Unretained(this)));
    Run();
  }

  void RunUpdateWebApk() {
    const int kWebApkVersion = 1;

    std::map<std::string, std::string> icon_url_to_murmur2_hash{
        {best_primary_icon_url_.spec(), "0"}};

    WebApkInstaller::UpdateAsyncForTesting(
        CreateWebApkInstaller(), kDownloadedWebApkPackageName, kWebApkVersion,
        icon_url_to_murmur2_hash, false /* is_manifest_stale */,
        base::Bind(&WebApkInstallerRunner::OnCompleted,
                   base::Unretained(this)));
    Run();
  }

  WebApkInstaller* CreateWebApkInstaller() {
    ShortcutInfo info(GURL::EmptyGURL());
    info.best_primary_icon_url = best_primary_icon_url_;

    // WebApkInstaller owns itself.
    WebApkInstaller* installer =
        new TestWebApkInstaller(browser_context_, info, SkBitmap(),
                                can_use_google_play_install_service_);
    installer->SetTimeoutMs(100);
    return installer;
  }

  void Run() {
    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  WebApkInstallResult result() { return result_; }

 private:
  void OnCompleted(WebApkInstallResult result,
                   bool relax_updates,
                   const std::string& webapk_package) {
    result_ = result;
    on_completed_callback_.Run();
  }

  content::BrowserContext* browser_context_;

  // The Web Manifest's icon URL.
  const GURL best_primary_icon_url_;

  // Called after the installation process has succeeded or failed.
  base::Closure on_completed_callback_;

  // The result of the installation process.
  WebApkInstallResult result_;

  // Whether Google Play Service can be used and the install delegate is
  // available.
  bool can_use_google_play_install_service_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstallerRunner);
};

// Builds a webapk::WebApkResponse with |download_url| as the WebAPK download
// URL.
std::unique_ptr<net::test_server::HttpResponse> BuildValidWebApkResponse(
    const GURL& download_url) {
  std::unique_ptr<webapk::WebApkResponse> response_proto(
      new webapk::WebApkResponse);
  response_proto->set_package_name(kDownloadedWebApkPackageName);
  response_proto->set_signed_download_url(download_url.spec());
  std::string response_content;
  response_proto->SerializeToString(&response_content);

  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_code(net::HTTP_OK);
  response->set_content(response_content);
  return std::move(response);
}

// Builds WebApk proto and blocks till done.
class BuildProtoRunner {
 public:
  explicit BuildProtoRunner(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}

  ~BuildProtoRunner() {}

  void BuildSync(
      const GURL& best_primary_icon_url,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale) {
    ShortcutInfo info(GURL::EmptyGURL());
    info.best_primary_icon_url = best_primary_icon_url;

    // WebApkInstaller owns itself.
    WebApkInstaller* installer =
        new TestWebApkInstaller(browser_context_, info, SkBitmap(), false);
    installer->BuildWebApkProtoInBackgroundForTesting(
        base::Bind(&BuildProtoRunner::OnBuiltWebApkProto,
                   base::Unretained(this)),
        icon_url_to_murmur2_hash, is_manifest_stale);

    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  webapk::WebApk* GetWebApkRequest() { return webapk_request_.get(); }

 private:
  // Called when the |webapk_request_| is populated.
  void OnBuiltWebApkProto(std::unique_ptr<webapk::WebApk> webapk) {
    webapk_request_ = std::move(webapk);
    on_completed_callback_.Run();
  }

  content::BrowserContext* browser_context_;

  // The populated webapk::WebApk.
  std::unique_ptr<webapk::WebApk> webapk_request_;

  // Called after the |webapk_request_| is built.
  base::Closure on_completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(BuildProtoRunner);
};

}  // anonymous namespace

class WebApkInstallerTest : public ::testing::Test {
 public:
  typedef base::Callback<std::unique_ptr<net::test_server::HttpResponse>(void)>
      WebApkResponseBuilder;

  WebApkInstallerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~WebApkInstallerTest() override {}

  void SetUp() override {
    test_server_.AddDefaultHandlers(base::FilePath(kTestDataDir));
    test_server_.RegisterRequestHandler(base::Bind(
        &WebApkInstallerTest::HandleWebApkRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    profile_.reset(new TestingProfile());

    SetDefaults();
  }

  void TearDown() override {
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Sets the best Web Manifest's primary icon URL.
  void SetBestPrimaryIconUrl(const GURL& best_primary_icon_url) {
    best_primary_icon_url_ = best_primary_icon_url;
  }

  // Sets the URL to send the webapk::CreateWebApkRequest to. WebApkInstaller
  // should fail if the URL is not |kServerUrl|.
  void SetWebApkServerUrl(const GURL& server_url) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWebApkServerUrl, server_url.spec());
  }

  // Sets the function that should be used to build the response to the
  // WebAPK creation request.
  void SetWebApkResponseBuilder(const WebApkResponseBuilder& builder) {
    webapk_response_builder_ = builder;
  }

  std::unique_ptr<WebApkInstallerRunner> CreateWebApkInstallerRunner() {
    return std::unique_ptr<WebApkInstallerRunner>(
        new WebApkInstallerRunner(profile_.get(), best_primary_icon_url_));
  }

  std::unique_ptr<BuildProtoRunner> CreateBuildProtoRunner() {
    return std::unique_ptr<BuildProtoRunner>(
        new BuildProtoRunner(profile_.get()));
  }

  net::test_server::EmbeddedTestServer* test_server() { return &test_server_; }

 private:
  // Sets default configuration for running WebApkInstaller.
  void SetDefaults() {
    GURL best_primary_icon_url = test_server_.GetURL(kBestPrimaryIconUrl);
    SetBestPrimaryIconUrl(best_primary_icon_url);
    GURL server_url = test_server_.GetURL(kServerUrl);
    SetWebApkServerUrl(server_url);
    GURL download_url = test_server_.GetURL(kDownloadUrl);
    SetWebApkResponseBuilder(
        base::Bind(&BuildValidWebApkResponse, download_url));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleWebApkRequest(
      const net::test_server::HttpRequest& request) {
    return (request.relative_url == kServerUrl)
               ? webapk_response_builder_.Run()
               : std::unique_ptr<net::test_server::HttpResponse>();
  }

  std::unique_ptr<TestingProfile> profile_;
  content::TestBrowserThreadBundle thread_bundle_;
  net::EmbeddedTestServer test_server_;

  // Web Manifest's icon URL.
  GURL best_primary_icon_url_;

  // Builds response to the WebAPK creation request.
  WebApkResponseBuilder webapk_response_builder_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstallerTest);
};

// Test installation succeeding.
TEST_F(WebApkInstallerTest, Success) {
  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner->result());
}

// Test that installation fails if fetching the bitmap at the best primary icon
// URL times out. In a perfect world the fetch would never time out because the
// bitmap at the best primary icon URL should be in the HTTP cache.
TEST_F(WebApkInstallerTest, BestPrimaryIconUrlDownloadTimesOut) {
  GURL best_primary_icon_url = test_server()->GetURL("/slow?1000");
  SetBestPrimaryIconUrl(best_primary_icon_url);

  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner->result());
}

// Test that installation fails if the WebAPK creation request times out.
TEST_F(WebApkInstallerTest, CreateWebApkRequestTimesOut) {
  GURL server_url = test_server()->GetURL("/slow?1000");
  SetWebApkServerUrl(server_url);

  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner->result());
}

// Test that installation fails if the WebAPK download times out.
TEST_F(WebApkInstallerTest, WebApkDownloadTimesOut) {
  GURL download_url = test_server()->GetURL("/slow?1000");
  SetWebApkResponseBuilder(base::Bind(&BuildValidWebApkResponse, download_url));

  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner->result());
}

// Test that installation fails if the WebAPK download fails.
TEST_F(WebApkInstallerTest, WebApkDownloadFails) {
  GURL download_url = test_server()->GetURL("/nocontent");
  SetWebApkResponseBuilder(base::Bind(&BuildValidWebApkResponse, download_url));

  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner->result());
}

namespace {

// Returns an HttpResponse which cannot be parsed as a webapk::WebApkResponse.
std::unique_ptr<net::test_server::HttpResponse>
BuildUnparsableWebApkResponse() {
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_code(net::HTTP_OK);
  response->set_content("😀");
  return std::move(response);
}

}  // anonymous namespace

// Test that an HTTP response which cannot be parsed as a webapk::WebApkResponse
// is handled properly.
TEST_F(WebApkInstallerTest, UnparsableCreateWebApkResponse) {
  SetWebApkResponseBuilder(base::Bind(&BuildUnparsableWebApkResponse));

  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner->result());
}

// Test update succeeding.
TEST_F(WebApkInstallerTest, UpdateSuccess) {
  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunUpdateWebApk();
  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner->result());
}

// Test that an update suceeds if the WebAPK server returns a HTTP response with
// an empty download URL. The WebAPK server sends an empty download URL when:
// - The server is unable to update the WebAPK in the way that the client
//   requested.
// AND
// - The most up to date version of the WebAPK on the server is identical to the
//   one installed on the client.
TEST_F(WebApkInstallerTest, UpdateSuccessWithEmptyDownloadUrlInResponse) {
  SetWebApkResponseBuilder(base::Bind(&BuildValidWebApkResponse, GURL()));

  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunUpdateWebApk();
  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner->result());
}

// Test installation succeeds using Google Play.
TEST_F(WebApkInstallerTest, InstallFromGooglePlaySuccess) {
  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->SetCanUseGooglePlayInstallService(true);
  runner->RunInstallWebApk();
  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner->result());
}

// When there is no Web Manifest available for a site, an empty
// |best_primary_icon_url| is used to build a WebApk update request. Tests the
// request can be built properly.
TEST_F(WebApkInstallerTest, BuildWebApkProtoWhenManifestIsObsolete) {
  GURL icon_url_1 = test_server()->GetURL("/icon1.png");
  GURL icon_url_2 = test_server()->GetURL("/icon2.png");
  std::string icon_murmur2_hash_1 = "1";
  std::string icon_murmur2_hash_2 = "2";
  std::map<std::string, std::string> icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1.spec()] = icon_murmur2_hash_1;
  icon_url_to_murmur2_hash[icon_url_2.spec()] = icon_murmur2_hash_2;

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(""), icon_url_to_murmur2_hash,
                    true /* is_manifest_stale*/);
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  webapk::Image icons[3];
  for (int i = 0; i < 3; ++i)
    icons[i] = manifest.icons(i);

  EXPECT_EQ("", icons[0].src());
  EXPECT_FALSE(icons[0].has_hash());
  EXPECT_TRUE(icons[0].has_image_data());

  EXPECT_EQ(icon_url_1.spec(), icons[1].src());
  EXPECT_EQ(icon_murmur2_hash_1, icons[1].hash());
  EXPECT_FALSE(icons[1].has_image_data());

  EXPECT_EQ(icon_url_2.spec(), icons[2].src());
  EXPECT_EQ(icon_murmur2_hash_2, icons[2].hash());
  EXPECT_FALSE(icons[2].has_image_data());
}

// Tests a WebApk install or update request is built properly when the Chrome
// knows the best icon URL of a site after fetching its Web Manifest.
TEST_F(WebApkInstallerTest, BuildWebApkProtoWhenManifestIsAvailable) {
  GURL icon_url_1 = test_server()->GetURL("/icon.png");
  GURL best_primary_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::string icon_murmur2_hash_1 = "1";
  std::string best_primary_icon_murmur2_hash = "0";
  std::map<std::string, std::string> icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1.spec()] = icon_murmur2_hash_1;
  icon_url_to_murmur2_hash[best_primary_icon_url.spec()] =
      best_primary_icon_murmur2_hash;

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(best_primary_icon_url, icon_url_to_murmur2_hash,
                    false /* is_manifest_stale*/);
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(2, manifest.icons_size());

  webapk::Image icons[2];
  for (int i = 0; i < 2; ++i)
    icons[i] = manifest.icons(i);

  EXPECT_EQ(best_primary_icon_url.spec(), icons[0].src());
  EXPECT_EQ(best_primary_icon_murmur2_hash, icons[0].hash());
  EXPECT_TRUE(icons[0].has_image_data());

  EXPECT_EQ(icon_url_1.spec(), icons[1].src());
  EXPECT_EQ(icon_murmur2_hash_1, icons[1].hash());
  EXPECT_FALSE(icons[1].has_image_data());
}
