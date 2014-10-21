// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_WEB_APP_H_
#define CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_WEB_APP_H_

#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Helper method to OpenApplication() which opens a web app window with |url|
// in the way specified by |params|.
content::WebContents* OpenWebAppWindow(const AppLaunchParams& params,
                                       const GURL& url);

// Helper method to OpenApplication() which opens a web app tab with |url| in
// the way specified by |params|.
content::WebContents* OpenWebAppTab(const AppLaunchParams& params,
                                    const GURL& url);

#endif  // CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_WEB_APP_H_
