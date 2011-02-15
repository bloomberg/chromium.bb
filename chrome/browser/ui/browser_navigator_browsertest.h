// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"

class GURL;
class NotificationDetails;
class NotificationSource;
class Profile;
class TabContentsWrapper;

namespace browser {
struct NavigateParams;
}

// Browsertest class for testing the browser navigation. It is also a base class
// for the |BrowserGuestModeNavigation| which tests navigation while in guest
// mode.
class BrowserNavigatorTest : public InProcessBrowserTest,
                             public NotificationObserver {
 protected:
  GURL GetGoogleURL() const;

  browser::NavigateParams MakeNavigateParams() const;
  browser::NavigateParams MakeNavigateParams(Browser* browser) const;

  Browser* CreateEmptyBrowserForType(Browser::Type type, Profile* profile);

  TabContentsWrapper* CreateTabContents();

  void RunSuppressTest(WindowOpenDisposition disposition);

  // NotificationObserver:
  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details);

  size_t created_tab_contents_count_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_
