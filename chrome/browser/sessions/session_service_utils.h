// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_UTILS_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_UTILS_H_

#include "chrome/browser/ui/browser.h"
#include "components/sessions/session_types.h"

// The maximum number of navigation entries in each direction we'll persist for
// chrome/Browser.
extern const int gMaxPersistNavigationCount;

// Convert back/forward between the Browser and SessionService window types.
sessions::SessionWindow::WindowType WindowTypeForBrowserType(
    Browser::Type type);
Browser::Type BrowserTypeForWindowType(
    sessions::SessionWindow::WindowType type);

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_UTILS_H_
