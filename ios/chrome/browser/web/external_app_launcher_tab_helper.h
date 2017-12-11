// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTERNAL_APP_LAUNCHER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_EXTERNAL_APP_LAUNCHER_TAB_HELPER_H_

#include "base/macros.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@class ExternalAppLauncher;
class GURL;

// Handles the UI behaviors when opening a URL in an external application.
class ExternalAppLauncherTabHelper
    : public web::WebStateUserData<ExternalAppLauncherTabHelper> {
 public:
  ~ExternalAppLauncherTabHelper() override;

  // Requests to open URL in an external application.
  // Returns NO if |url| is invalid, or the application for |url| is not
  // available, or it is not possible to open the application for |url|.
  // If the application for |url| has been opened repeatedly by
  // |source_page_url| page in a short time frame, the user will be prompted
  // with an option to block the application from launching.
  // The user may be prompted with a confirmation dialog to open the application
  // for certain schemes (e.g., facetime and mailto). |link_clicked| indicates
  // whether the user tapped on the link element.
  BOOL RequestToOpenUrl(const GURL& url,
                        const GURL& source_page_url,
                        BOOL link_clicked);

 private:
  friend class web::WebStateUserData<ExternalAppLauncherTabHelper>;
  explicit ExternalAppLauncherTabHelper(web::WebState* web_state);

  ExternalAppLauncher* launcher_;

  DISALLOW_COPY_AND_ASSIGN(ExternalAppLauncherTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_EXTERNAL_APP_LAUNCHER_TAB_HELPER_H_
