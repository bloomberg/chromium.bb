// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_browser_test.h"

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "headless/public/headless_web_contents.h"

namespace headless {
namespace {

class WaitForNavigationObserver : public HeadlessWebContents::Observer {
 public:
  WaitForNavigationObserver(base::RunLoop* run_loop,
                            HeadlessWebContents* web_contents)
      : run_loop_(run_loop),
        web_contents_(web_contents),
        navigation_succeeded_(false) {
    web_contents_->AddObserver(this);
  }

  ~WaitForNavigationObserver() override { web_contents_->RemoveObserver(this); }

  void DocumentOnLoadCompletedInMainFrame() override { run_loop_->Quit(); }
  void DidFinishNavigation(bool success) override {
    navigation_succeeded_ = success;
  }

  bool navigation_succeeded() const { return navigation_succeeded_; }

 private:
  base::RunLoop* run_loop_;            // Not owned.
  HeadlessWebContents* web_contents_;  // Not owned.

  bool navigation_succeeded_;

  DISALLOW_COPY_AND_ASSIGN(WaitForNavigationObserver);
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

bool HeadlessBrowserTest::NavigateAndWaitForLoad(
    HeadlessWebContents* web_contents,
    const GURL& url) {
  base::RunLoop run_loop;
  base::MessageLoop::ScopedNestableTaskAllower nestable_allower(
      base::MessageLoop::current());
  WaitForNavigationObserver observer(&run_loop, web_contents);

  if (!web_contents->OpenURL(url))
    return false;
  run_loop.Run();
  return observer.navigation_succeeded();
}

}  // namespace headless
