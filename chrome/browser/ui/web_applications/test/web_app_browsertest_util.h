// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_

#include "chrome/browser/web_applications/components/web_app_helpers.h"

class Browser;
class Profile;

namespace web_app {

struct ExternalInstallOptions;
enum class InstallResultCode;

// Launches a new app window for |app| in |profile|.
Browser* LaunchWebAppBrowser(Profile*, const AppId&);

// Launches a new tab for |app| in |profile|.
Browser* LaunchBrowserForWebAppInTab(Profile*, const AppId&);

ExternalInstallOptions CreateInstallOptions(const GURL& url);

InstallResultCode InstallApp(Profile*, ExternalInstallOptions);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_
