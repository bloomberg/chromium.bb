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
#include "chrome/common/chrome_switches.h"
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

// The best icon URL from Web Manifest. We use a random file in the test data
// directory. Since WebApkInstaller does not try to decode the file as an image
// it is OK that the file is not an image.
const char* kBestIconUrl = "/simple.html";

// URL of file to download from the WebAPK server. We use a random file in the
// test data directory.
const char* kDownloadUrl = "/simple.html";

// The package name of the downloaded WebAPK.
const char* kDownloadedWebApkPackageName = "party.unicode";

// WebApkInstaller subclass where
// WebApkInstaller::StartInstallingDownloadedWebApk() and
// WebApkInstaller::StartUpdateUsingDownloadedWebApk() are stubbed out.
class TestWebApkInstaller : public WebApkInstaller {
 public:
  TestWebApkInstaller(const ShortcutInfo& shortcut_info,
                      const SkBitmap& shortcut_icon)
      : WebApkInstaller(shortcut_info, shortcut_icon) {}

  bool StartInstallingDownloadedWebApk(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jstring>& file_path,
      const base::android::ScopedJavaLocalRef<jstring>& package_name) override {
    PostTaskToRunSuccessCallback();
    return true;
  }

  bool StartUpdateUsingDownloadedWebApk(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jstring>& file_path) override {
    return true;
  }

  void PostTaskToRunSuccessCallback() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&TestWebApkInstaller::OnSuccess, base::Unretained(this)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebApkInstaller);
};

// Runs the WebApkInstaller installation process/update and blocks till done.
class WebApkInstallerRunner {
 public:
  explicit WebApkInstallerRunner(const GURL& best_icon_url)
      : url_request_context_getter_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        best_icon_url_(best_icon_url) {}
  ~WebApkInstallerRunner() {}

  void RunInstallWebApk() {
    WebApkInstaller* installer = CreateWebApkInstaller();

    installer->InstallAsyncWithURLRequestContextGetter(
        url_request_context_getter_.get(),
        base::Bind(&WebApkInstallerRunner::OnCompleted,
                   base::Unretained(this)));
    Run();
  }

  void RunUpdateWebApk() {
    const std::string kIconMurmur2Hash = "0";
    const int kWebApkVersion = 1;

    WebApkInstaller* installer = CreateWebApkInstaller();

    installer->UpdateAsyncWithURLRequestContextGetter(
        url_request_context_getter_.get(),
        base::Bind(&WebApkInstallerRunner::OnCompleted, base::Unretained(this)),
        kIconMurmur2Hash,
        kDownloadedWebApkPackageName,
        kWebApkVersion);

    Run();
  }

  WebApkInstaller* CreateWebApkInstaller() {
    ShortcutInfo info(GURL::EmptyGURL());
    info.best_icon_url = best_icon_url_;

    // WebApkInstaller owns itself.
    WebApkInstaller* installer = new TestWebApkInstaller(info, SkBitmap());
    installer->SetTimeoutMs(100);
    return installer;
  }

  void Run() {
    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool success() { return success_; }

 private:
  void OnCompleted(bool success, const std::string& webapk_package) {
    success_ = success;
    on_completed_callback_.Run();
  }

  scoped_refptr<net::TestURLRequestContextGetter>
      url_request_context_getter_;

  // The Web Manifest's icon URL.
  const GURL best_icon_url_;

  // Called after the installation process has succeeded or failed.
  base::Closure on_completed_callback_;

  // Whether the installation process succeeded.
  bool success_;

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
    test_server_.RegisterRequestHandler(
        base::Bind(&WebApkInstallerTest::HandleWebApkRequest,
                   base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    SetDefaults();
  }

  // Sets the best Web Manifest's icon URL.
  void SetBestIconUrl(const GURL& best_icon_url) {
    best_icon_url_ = best_icon_url;
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
        new WebApkInstallerRunner(best_icon_url_));
  }

  net::test_server::EmbeddedTestServer* test_server() { return &test_server_; }

 private:
  // Sets default configuration for running WebApkInstaller.
  void SetDefaults() {
    GURL best_icon_url = test_server_.GetURL(kBestIconUrl);
    SetBestIconUrl(best_icon_url);
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

  content::TestBrowserThreadBundle thread_bundle_;
  net::EmbeddedTestServer test_server_;

  // Web Manifest's icon URL.
  GURL best_icon_url_;

  // Builds response to the WebAPK creation request.
  WebApkResponseBuilder webapk_response_builder_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstallerTest);
};

// Test installation succeeding.
TEST_F(WebApkInstallerTest, Success) {
  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_TRUE(runner->success());
}

// Test that installation fails if fetching the bitmap at the best icon URL
// times out. In a perfect world the fetch would never time out because the
// bitmap at the best icon URL should be in the HTTP cache.
TEST_F(WebApkInstallerTest, BestIconUrlDownloadTimesOut) {
  GURL best_icon_url = test_server()->GetURL("/slow?1000");
  SetBestIconUrl(best_icon_url);

  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_FALSE(runner->success());
}

// Test that installation fails if the WebAPK creation request times out.
TEST_F(WebApkInstallerTest, CreateWebApkRequestTimesOut) {
  GURL server_url = test_server()->GetURL("/slow?1000");
  SetWebApkServerUrl(server_url);

  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_FALSE(runner->success());
}

// Test that installation fails if the WebAPK download times out.
TEST_F(WebApkInstallerTest, WebApkDownloadTimesOut) {
  GURL download_url = test_server()->GetURL("/slow?1000");
  SetWebApkResponseBuilder(base::Bind(&BuildValidWebApkResponse, download_url));

  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_FALSE(runner->success());
}

// Test that installation fails if the WebAPK download fails.
TEST_F(WebApkInstallerTest, WebApkDownloadFails) {
  GURL download_url = test_server()->GetURL("/nocontent");
  SetWebApkResponseBuilder(base::Bind(&BuildValidWebApkResponse, download_url));

  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunInstallWebApk();
  EXPECT_FALSE(runner->success());
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
  EXPECT_FALSE(runner->success());
}

// Test update succeeding.
TEST_F(WebApkInstallerTest, UpdateSuccess) {
  std::unique_ptr<WebApkInstallerRunner> runner = CreateWebApkInstallerRunner();
  runner->RunUpdateWebApk();
  EXPECT_TRUE(runner->success());
}
