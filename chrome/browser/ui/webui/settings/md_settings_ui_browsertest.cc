// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

typedef InProcessBrowserTest MdSettingsBrowserTest;

using ui_test_utils::NavigateToURL;
using content::WaitForLoadStop;

IN_PROC_BROWSER_TEST_F(MdSettingsBrowserTest, ViewSourceDoesntCrash) {
  NavigateToURL(browser(),
      GURL(content::kViewSourceScheme + std::string(":") +
           chrome::kChromeUIMdSettingsURL));
}

// May not complete on memory bots. TODO(dbeam): investigate and fix.
// See https://crbug.com/558434 and https://crbug.com/620370.
#if defined(MEMORY_SANITIZER)
#define MAYBE_BackForwardDoesntCrash DISABLED_BackForwardDoesntCrash
#else
#define MAYBE_BackForwardDoesntCrash BackForwardDoesntCrash
#endif

IN_PROC_BROWSER_TEST_F(MdSettingsBrowserTest, MAYBE_BackForwardDoesntCrash) {
  NavigateToURL(browser(), GURL(chrome::kChromeUIMdSettingsURL));

  NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  chrome::GoBack(browser(), CURRENT_TAB);
  WaitForLoadStop(browser()->tab_strip_model()->GetActiveWebContents());
}
