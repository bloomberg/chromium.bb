// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/chrome_visibility_observer.h"

#include "base/memory/singleton.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

// Mock class for |ChromeVisibilityObserver| for testing.
class MockChromeVisibilityObserver : public metrics::ChromeVisibilityObserver {
 public:
  MockChromeVisibilityObserver() : is_active_(false) {
    ResetRunLoop();
  }

  void Wait() {
    run_loop_->Run();
  }

  void ResetRunLoop() {
    run_loop_.reset(new base::RunLoop());
  }

  bool is_active() const { return is_active_; }
  void set_is_active(bool is_active) { is_active_ = is_active; }

 private:
  void SendVisibilityChangeEvent(bool active,
                                 base::TimeDelta time_ago) override {
    is_active_ = active;
    // In this test browser became inactive after browser is removed, thus
    // in this case quit runloop in OnBrowserRemoved().
    if (is_active_)
      run_loop_->Quit();
  }

  void OnBrowserRemoved(Browser* browser) override {
    metrics::ChromeVisibilityObserver::OnBrowserRemoved(browser);
    run_loop_->Quit();
  }

  bool is_active_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MockChromeVisibilityObserver);
};

class ChromeVisibilityObserverBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(ChromeVisibilityObserverBrowserTest, VisibilityTest) {
  MockChromeVisibilityObserver observer;

  EXPECT_FALSE(observer.is_active());
  Browser* new_browser = CreateBrowser(browser()->profile());
  observer.Wait();
  EXPECT_TRUE(observer.is_active());

  observer.ResetRunLoop();
  observer.set_is_active(false);
  Browser* incognito_browser = CreateIncognitoBrowser();
  observer.Wait();
  EXPECT_TRUE(observer.is_active());

  observer.ResetRunLoop();
  CloseBrowserSynchronously(incognito_browser);
  observer.Wait();
  EXPECT_TRUE(observer.is_active());

  observer.ResetRunLoop();
  CloseBrowserSynchronously(new_browser);
  observer.Wait();
  EXPECT_TRUE(observer.is_active());

  observer.ResetRunLoop();
  CloseBrowserSynchronously(browser());
  observer.Wait();
  EXPECT_FALSE(observer.is_active());
}
