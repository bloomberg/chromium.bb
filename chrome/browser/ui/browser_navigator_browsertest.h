// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_types.h"

class Profile;

namespace base {
class CommandLine;
}

namespace chrome {
struct NavigateParams;
}

namespace content {
class WebContents;
}

// Browsertest class for testing the browser navigation. It is also a base class
// for the |BrowserGuestModeNavigation| which tests navigation while in guest
// mode.
class BrowserNavigatorTest : public InProcessBrowserTest,
                             public content::NotificationObserver {
 protected:
  chrome::NavigateParams MakeNavigateParams() const;
  chrome::NavigateParams MakeNavigateParams(Browser* browser) const;

  Browser* CreateEmptyBrowserForType(Browser::Type type, Profile* profile);
  Browser* CreateEmptyBrowserForApp(Profile* profile);

  content::WebContents* CreateWebContents();

  void RunSuppressTest(WindowOpenDisposition disposition);
  void RunUseNonIncognitoWindowTest(const GURL& url);
  void RunDoNothingIfIncognitoIsForcedTest(const GURL& url);

  // InProcessBrowserTest:
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  bool OpenPOSTURLInNewForegroundTabAndGetTitle(const GURL& url,
                                                const std::string& post_data,
                                                bool is_browser_initiated,
                                                base::string16* title);

  size_t created_tab_contents_count_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_BROWSERTEST_H_
