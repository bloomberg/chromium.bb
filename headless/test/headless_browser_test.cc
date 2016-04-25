// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_browser_test.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "headless/public/domains/network.h"
#include "headless/public/domains/page.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"

namespace headless {
namespace {

class WaitForLoadObserver : public page::Observer, public network::Observer {
 public:
  WaitForLoadObserver(HeadlessBrowserTest* browser_test,
                      HeadlessWebContents* web_contents)
      : browser_test_(browser_test),
        web_contents_(web_contents),
        devtools_client_(HeadlessDevToolsClient::Create()),
        navigation_succeeded_(true) {
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    devtools_client_->GetNetwork()->AddObserver(this);
    devtools_client_->GetNetwork()->Enable();
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
  }

  ~WaitForLoadObserver() override {
    devtools_client_->GetNetwork()->RemoveObserver(this);
    devtools_client_->GetPage()->RemoveObserver(this);
    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  }

  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    browser_test_->FinishAsynchronousTest();
  }

  void OnResponseReceived(
      const network::ResponseReceivedParams& params) override {
    if (params.GetResponse()->GetStatus() != 200 ||
        params.GetResponse()->GetUrl() == content::kUnreachableWebDataURL) {
      navigation_succeeded_ = false;
    }
  }

  bool navigation_succeeded() const { return navigation_succeeded_; }

 private:
  HeadlessBrowserTest* browser_test_;  // Not owned.
  HeadlessWebContents* web_contents_;  // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;

  bool navigation_succeeded_;

  DISALLOW_COPY_AND_ASSIGN(WaitForLoadObserver);
};

}  // namespace

HeadlessBrowserTest::HeadlessBrowserTest() {
  base::FilePath headless_test_data(FILE_PATH_LITERAL("headless/test/data"));
  CreateTestServer(headless_test_data);
}

HeadlessBrowserTest::~HeadlessBrowserTest() {}

void HeadlessBrowserTest::SetUpOnMainThread() {}

void HeadlessBrowserTest::TearDownOnMainThread() {
  browser()->Shutdown();
}

void HeadlessBrowserTest::RunTestOnMainThreadLoop() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Pump startup related events.
  base::MessageLoop::current()->RunUntilIdle();

  SetUpOnMainThread();
  RunTestOnMainThread();
  TearDownOnMainThread();

  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }
}

void HeadlessBrowserTest::SetBrowserOptions(
    const HeadlessBrowser::Options& options) {
  HeadlessContentMainDelegate::GetInstance()->browser()->SetOptionsForTesting(
      options);
}

HeadlessBrowser* HeadlessBrowserTest::browser() const {
  return HeadlessContentMainDelegate::GetInstance()->browser();
}

bool HeadlessBrowserTest::WaitForLoad(HeadlessWebContents* web_contents) {
  WaitForLoadObserver observer(this, web_contents);
  RunAsynchronousTest();
  return observer.navigation_succeeded();
}

void HeadlessBrowserTest::RunAsynchronousTest() {
  base::MessageLoop::ScopedNestableTaskAllower nestable_allower(
      base::MessageLoop::current());
  EXPECT_FALSE(run_loop_);
  run_loop_ = base::WrapUnique(new base::RunLoop());
  run_loop_->Run();
  run_loop_ = nullptr;
}

void HeadlessBrowserTest::FinishAsynchronousTest() {
  run_loop_->Quit();
}

}  // namespace headless
