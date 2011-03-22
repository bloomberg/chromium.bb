// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"

namespace {

class NavigationNotificationObserver : public NotificationObserver {
 public:
  NavigationNotificationObserver()
      : got_navigation_(false),
        http_status_code_(0) {
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(NotificationType::NAV_ENTRY_COMMITTED, type.value);
    got_navigation_ = true;
    http_status_code_ =
        Details<NavigationController::LoadCommittedDetails>(details)->
        http_status_code;
  }

  int http_status_code() const { return http_status_code_; }
  bool got_navigation() const { return got_navigation_; }

  private:
  NotificationRegistrar registrar_;
  int got_navigation_;
  int http_status_code_;

  DISALLOW_COPY_AND_ASSIGN(NavigationNotificationObserver);
};

}  // namespace

typedef InProcessBrowserTest ChromeURLDataManagerTest;

// Makes sure navigating to the new tab page results in a http status code
// of 200.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, 200) {
  NavigationNotificationObserver observer;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_TRUE(observer.got_navigation());
  EXPECT_EQ(200, observer.http_status_code());
}
