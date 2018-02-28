// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/chrome_visibility_observer.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_utils.h"

// Mock class for |ChromeVisibilityObserver| for testing.
class MockChromeVisibilityObserver : public metrics::ChromeVisibilityObserver {
 public:
  MockChromeVisibilityObserver() : is_active_(false) {}

  bool is_active() const { return is_active_; }

 private:
  void SendVisibilityChangeEvent(bool active,
                                 base::TimeDelta time_ago) override {
    is_active_ = active;
  }

  bool is_active_;

  DISALLOW_COPY_AND_ASSIGN(MockChromeVisibilityObserver);
};

class ChromeVisibilityObserverBrowserTest : public InProcessBrowserTest {};

// TODO(https://crbug.com/817172): This test is flaky.
IN_PROC_BROWSER_TEST_F(ChromeVisibilityObserverBrowserTest,
                       DISABLED_VisibilityTest) {
  // Deactivate the initial browser window, to make observer start inactive.
  browser()->window()->Deactivate();
  base::RunLoop().RunUntilIdle();

  MockChromeVisibilityObserver observer;

  EXPECT_FALSE(observer.is_active());
  Browser* new_browser = CreateBrowser(browser()->profile());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.is_active());

  Browser* incognito_browser = CreateIncognitoBrowser();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.is_active());

  CloseBrowserSynchronously(incognito_browser);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.is_active());

  CloseBrowserSynchronously(new_browser);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.is_active());

  CloseBrowserSynchronously(browser());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observer.is_active());
}
