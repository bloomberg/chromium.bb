// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"

typedef options::OptionsUIBrowserTest ContentSettingsExceptionsAreaBrowserTest;

// Test that an incognito window can be opened while the exceptions page is
// open. If this test fails it could indicate that a new content setting has
// been added but is not being dealt with correctly by the content settings
// handling WebUI code.
#if defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
// Flaky on ASAN on Mac. See https://crbug.com/674497.
#define MAYBE_OpenIncognitoWindow DISABLED_OpenIncognitoWindow
#else
#define MAYBE_OpenIncognitoWindow OpenIncognitoWindow
#endif
IN_PROC_BROWSER_TEST_F(ContentSettingsExceptionsAreaBrowserTest,
                       MAYBE_OpenIncognitoWindow) {
  NavigateToSettingsSubpage("contentExceptions");
  chrome::NewIncognitoWindow(browser());
}
