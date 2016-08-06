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
const std::string kServerUrl = "/webapkserver";

// URL of file to download from the WebAPK server. We use a random file in the
// test data directory.
const std::string kDownloadUrl = "/simple.html";

// The package name of the downloaded WebAPK.
const std::string kDownloadedWebApkPackageName = "party.unicode";

// WebApkInstaller subclass where
// WebApkInstaller::StartDownloadedWebApkInstall() is stubbed out.
class TestWebApkInstaller : public WebApkInstaller {
 public:
  TestWebApkInstaller(const ShortcutInfo& shortcut_info,
                      const SkBitmap& shortcut_icon)
      : WebApkInstaller(shortcut_info, shortcut_icon) {}

  bool StartDownloadedWebApkInstall(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jstring>& file_path,
      const base::android::ScopedJavaLocalRef<jstring>& package_name) override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebApkInstaller);
};

// Runs the WebApkInstaller installation process and blocks till done.
class WebApkInstallerRunner {
 public:
  WebApkInstallerRunner()
      : url_request_context_getter_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())) {}
  ~WebApkInstallerRunner() {}

  void Run() {
    ShortcutInfo info(GURL::EmptyGURL());

    // WebApkInstaller owns itself.
    WebApkInstaller* installer = new TestWebApkInstaller(info, SkBitmap());

    installer->InstallAsyncWithURLRequestContextGetter(
        url_request_context_getter_.get(),
        base::Bind(&WebApkInstallerRunner::OnCompleted,
                   base::Unretained(this)));

    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool success() { return success_; }

 private:
  void OnCompleted(bool success) {
    success_ = success;
    on_completed_callback_.Run();
  }

  scoped_refptr<net::TestURLRequestContextGetter>
      url_request_context_getter_;

  // Called after the installation process has succeeded or failed.
  base::Closure on_completed_callback_;

  // Whether the installation process succeeded.
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstallerRunner);
};

// Builds a webapk::CreateWebApkResponse with |download_url| as the WebAPK
// download URL.
std::unique_ptr<net::test_server::HttpResponse> BuildValidCreateWebApkResponse(
    const GURL& download_url) {
  std::unique_ptr<webapk::CreateWebApkResponse> response_proto(
      new webapk::CreateWebApkResponse);
  response_proto->set_webapk_package_name(kDownloadedWebApkPackageName);
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
      CreateWebApkResponseBuilder;

  WebApkInstallerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~WebApkInstallerTest() override {}

  void SetUp() override {
    test_server_.AddDefaultHandlers(base::FilePath(kTestDataDir));
    test_server_.RegisterRequestHandler(
        base::Bind(&WebApkInstallerTest::HandleCreateWebApkRequest,
                   base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    SetDefaults();
  }

  // Sets the URL to send the webapk::CreateWebApkRequest to. WebApkInstaller
  // should fail if the URL is not |kServerUrl|.
  void SetWebApkServerUrl(const GURL& server_url) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWebApkServerUrl, server_url.spec());
  }

  // Sets the function that should be used to build the response to the
  // webapk::CreateWebApkRequest.
  void SetCreateWebApkResponseBuilder(
      const CreateWebApkResponseBuilder& builder) {
    create_webapk_response_builder_ = builder;
  }

  net::test_server::EmbeddedTestServer* test_server() { return &test_server_; }

 private:
  // Sets default configuration for running WebApkInstaller.
  void SetDefaults() {
    GURL server_url = test_server_.GetURL(kServerUrl);
    SetWebApkServerUrl(server_url);
    GURL download_url = test_server_.GetURL(kDownloadUrl);
    SetCreateWebApkResponseBuilder(
        base::Bind(&BuildValidCreateWebApkResponse, download_url));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleCreateWebApkRequest(
      const net::test_server::HttpRequest& request) {
    return (request.relative_url == kServerUrl)
               ? create_webapk_response_builder_.Run()
               : std::unique_ptr<net::test_server::HttpResponse>();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  net::EmbeddedTestServer test_server_;

  // Builds response to the webapk::CreateWebApkRequest.
  CreateWebApkResponseBuilder create_webapk_response_builder_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstallerTest);
};

// Test installation succeeding.
TEST_F(WebApkInstallerTest, Success) {
  WebApkInstallerRunner runner;
  runner.Run();
  EXPECT_TRUE(runner.success());
}

// Test that installation fails if the CreateWebApkRequest times out.
TEST_F(WebApkInstallerTest, CreateWebApkRequestTimesOut) {
  GURL server_url = test_server()->GetURL("/slow?1000");
  SetWebApkServerUrl(server_url);

  WebApkInstallerRunner runner;
  runner.Run();
  EXPECT_FALSE(runner.success());
}

// Test that installation fails if the WebAPK download times out.
TEST_F(WebApkInstallerTest, WebApkDownloadTimesOut) {
  GURL download_url = test_server()->GetURL("/slow?1000");
  SetCreateWebApkResponseBuilder(
      base::Bind(&BuildValidCreateWebApkResponse, download_url));

  WebApkInstallerRunner runner;
  runner.Run();
  EXPECT_FALSE(runner.success());
}

// Test that installation fails if the WebAPK download fails.
TEST_F(WebApkInstallerTest, WebApkDownloadFails) {
  GURL download_url = test_server()->GetURL("/nocontent");
  SetCreateWebApkResponseBuilder(
      base::Bind(&BuildValidCreateWebApkResponse, download_url));

  WebApkInstallerRunner runner;
  runner.Run();
  EXPECT_FALSE(runner.success());
}

namespace {

// Returns an HttpResponse which cannot be parsed as a
// webapk::CreateWebApkResponse.
std::unique_ptr<net::test_server::HttpResponse>
BuildUnparsableCreateWebApkResponse() {
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_code(net::HTTP_OK);
  response->set_content("😀");
  return std::move(response);
}

}  // anonymous namespace

// Test that an HTTP response which cannot be parsed as
// a webapk::CreateWebApkResponse is handled properly.
TEST_F(WebApkInstallerTest, UnparsableCreateWebApkResponse) {
  SetCreateWebApkResponseBuilder(
      base::Bind(&BuildUnparsableCreateWebApkResponse));

  WebApkInstallerRunner runner;
  runner.Run();
  EXPECT_FALSE(runner.success());
}
