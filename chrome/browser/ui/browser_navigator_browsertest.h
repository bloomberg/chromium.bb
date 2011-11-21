// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_types.h"

class Profile;
class TabContentsWrapper;

namespace browser {
struct NavigateParams;
}

// Browsertest class for testing the browser navigation. It is also a base class
// for the |BrowserGuestModeNavigation| which tests navigation while in guest
// mode.
class BrowserNavigatorTest : public InProcessBrowserTest,
                             public content::NotificationObserver {
 protected:
  browser::NavigateParams MakeNavigateParams() const;
  browser::NavigateParams MakeNavigateParams(Browser* browser) const;

  Browser* CreateEmptyBrowserForType(Browser::Type type, Profile* profile);
  Browser* CreateEmptyBrowserForApp(Browser::Type type, Profile* profile);

  TabContentsWrapper* CreateTabContents();

  void RunSuppressTest(WindowOpenDisposition disposition);

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  size_t created_tab_contents_count_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_
