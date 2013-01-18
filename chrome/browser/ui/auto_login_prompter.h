// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTO_LOGIN_PROMPTER_H_
#define CHROME_BROWSER_UI_AUTO_LOGIN_PROMPTER_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/auto_login_info_bar_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;

namespace content {
class WebContents;
}

namespace net {
class URLRequest;
}

// This class displays an infobar that allows the user to automatically login to
// the currently loaded page with one click.  This is used when the browser
// detects that the user has navigated to a login page and that there are stored
// tokens that would allow a one-click login.
class AutoLoginPrompter : public content::NotificationObserver {
 public:
  typedef AutoLoginInfoBarDelegate::Params Params;

  // Looks for the X-Auto-Login response header in the request, and if found,
  // tries to display an infobar in the tab contents identified by the
  // child/route id.
  static void ShowInfoBarIfPossible(net::URLRequest* request,
                                    int child_id,
                                    int route_id);

 private:
  friend class AutoLoginPrompterTest;

  AutoLoginPrompter(content::WebContents* web_contents, const Params& params);

  virtual ~AutoLoginPrompter();

  static void ShowInfoBarUIThread(Params params,
                                  const GURL& url,
                                  int child_id,
                                  int route_id);

  // content::NotificationObserver override.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::WebContents* web_contents_;
  const Params params_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginPrompter);
};

#endif  // CHROME_BROWSER_UI_AUTO_LOGIN_PROMPTER_H_
