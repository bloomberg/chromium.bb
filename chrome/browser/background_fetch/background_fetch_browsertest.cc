// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/background_service/logger.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// URL of the test helper page that helps drive these tests.
const char kHelperPage[] = "/background_fetch/background_fetch.html";

// Name of the Background Fetch client as known by the download service.
const char kBackgroundFetchClient[] = "BackgroundFetch";

// Stringified values of request services made to the download service.
const char kResultAccepted[] = "ACCEPTED";

// Implementation of a download system logger that provides the ability to wait
// for certain events to happen, notably added and progressing downloads.
class WaitableDownloadLoggerObserver : public download::Logger::Observer {
 public:
  using DownloadAcceptedCallback =
      base::OnceCallback<void(const std::string& guid)>;

  WaitableDownloadLoggerObserver() = default;
  ~WaitableDownloadLoggerObserver() override = default;

  // Sets the |callback| to be invoked when a download has been accepted.
  void set_download_accepted_callback(DownloadAcceptedCallback callback) {
    download_accepted_callback_ = std::move(callback);
  }

  // download::Logger::Observer implementation:
  void OnServiceStatusChanged(const base::Value& service_status) override {}
  void OnServiceDownloadsAvailable(
      const base::Value& service_downloads) override {}
  void OnServiceDownloadChanged(const base::Value& service_download) override {}
  void OnServiceDownloadFailed(const base::Value& service_download) override {}
  void OnServiceRequestMade(const base::Value& service_request) override {
    const std::string& client = service_request.FindKey("client")->GetString();
    const std::string& guid = service_request.FindKey("guid")->GetString();
    const std::string& result = service_request.FindKey("result")->GetString();

    if (client != kBackgroundFetchClient)
      return;  // this event is not targeted to us

    if (result == kResultAccepted)
      std::move(download_accepted_callback_).Run(guid);
  }

 private:
  DownloadAcceptedCallback download_accepted_callback_;

  DISALLOW_COPY_AND_ASSIGN(WaitableDownloadLoggerObserver);
};

class BackgroundFetchBrowserTest : public InProcessBrowserTest {
 public:
  BackgroundFetchBrowserTest() = default;
  ~BackgroundFetchBrowserTest() override = default;

  // InProcessBrowserTest overrides:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Background Fetch is available as an experimental Web Platform feature.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_->Start());

    observer_ = std::make_unique<WaitableDownloadLoggerObserver>();

    download_service_ =
        DownloadServiceFactory::GetForBrowserContext(browser()->profile());
    download_service_->GetLogger()->AddObserver(observer_.get());

    // Load the helper page that helps drive these tests.
    ui_test_utils::NavigateToURL(browser(), https_server_->GetURL(kHelperPage));

    // Register the Service Worker that's required for Background Fetch. The
    // behaviour without an activated worker is covered by layout tests.
    {
      std::string script_result;
      ASSERT_TRUE(RunScript("RegisterServiceWorker()", &script_result));
      ASSERT_EQ("ok - service worker registered", script_result);
    }
  }

  void TearDownOnMainThread() override {
    download_service_->GetLogger()->RemoveObserver(observer_.get());
    download_service_ = nullptr;
  }

  // Runs the |script| in the current tab and writes the output to |*result|.
  bool RunScript(const std::string& script, std::string* result) {
    return content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        script, result);
  }

  // Runs the given |function| and asserts that it responds with "ok".
  // Must be wrapped with ASSERT_NO_FATAL_FAILURE().
  void RunScriptFunction(const std::string& function) {
    std::string result;
    ASSERT_TRUE(RunScript(function, &result));
    ASSERT_EQ("ok", result);
  }

  // Callback for WaitableDownloadLoggerObserver::DownloadAcceptedCallback().
  void DidAcceptDownloadCallback(base::OnceClosure quit_closure,
                                 std::string* out_guid,
                                 const std::string& guid) {
    DCHECK(out_guid);
    *out_guid = guid;

    std::move(quit_closure).Run();
  }

 protected:
  download::DownloadService* download_service_{nullptr};
  std::unique_ptr<WaitableDownloadLoggerObserver> observer_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest, DownloadSingleFile) {
  // Starts a Background Fetch for a single to-be-downloaded file and waits for
  // that request to be scheduled with the Download Service.

  std::string guid;
  {
    base::RunLoop run_loop;
    observer_->set_download_accepted_callback(
        base::BindOnce(&BackgroundFetchBrowserTest::DidAcceptDownloadCallback,
                       base::Unretained(this), run_loop.QuitClosure(), &guid));

    ASSERT_NO_FATAL_FAILURE(RunScriptFunction("StartSingleFileDownload()"));
    run_loop.Run();
  }

  EXPECT_FALSE(guid.empty());
}

}  // namespace
