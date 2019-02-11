// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_

class Browser;

namespace web_app {

// Returns true if a WebApp installation is allowed for the current page.
bool CanCreateWebApp(const Browser* browser);

// Initiates install of a WebApp for the current page.
void CreateWebAppFromCurrentWebContents(Browser* browser,
                                        bool force_shortcut_app);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
