// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_engagement/chrome_visibility_observer.h"

#include "base/memory/singleton.h"
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
  MockChromeVisibilityObserver() : is_active_(false) {}

  bool is_active() const { return is_active_; }

 private:
  void SendVisibilityChangeEvent(bool active) override { is_active_ = active; }

  bool is_active_;

  DISALLOW_COPY_AND_ASSIGN(MockChromeVisibilityObserver);
};

class ChromeVisibilityObserverBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(ChromeVisibilityObserverBrowserTest, VisibilityTest) {
  MockChromeVisibilityObserver observer;

  EXPECT_FALSE(observer.is_active());
  Browser* new_browser = CreateBrowser(browser()->profile());
  EXPECT_TRUE(observer.is_active());

  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_TRUE(observer.is_active());

  CloseBrowserSynchronously(incognito_browser);
  EXPECT_TRUE(observer.is_active());

  CloseBrowserSynchronously(new_browser);
  CloseBrowserSynchronously(browser());
  EXPECT_FALSE(observer.is_active());
}
