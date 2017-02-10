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
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

typedef InProcessBrowserTest MdSettingsUITest;

using ui_test_utils::NavigateToURL;
using content::WaitForLoadStop;

IN_PROC_BROWSER_TEST_F(MdSettingsUITest, ViewSourceDoesntCrash) {
  NavigateToURL(browser(), GURL(content::kViewSourceScheme + std::string(":") +
                                chrome::kChromeUIMdSettingsURL +
                                std::string("strings.js")));
}

// May not complete on memory and Windows debug bots. TODO(dbeam): investigate
// and fix. See https://crbug.com/558434, https://crbug.com/620370 and
// https://crbug.com/651296.
#if defined(MEMORY_SANITIZER) || defined(OS_WIN) || defined(OS_CHROMEOS)
#define MAYBE_BackForwardDoesntCrash DISABLED_BackForwardDoesntCrash
#else
#define MAYBE_BackForwardDoesntCrash BackForwardDoesntCrash
#endif

IN_PROC_BROWSER_TEST_F(MdSettingsUITest, MAYBE_BackForwardDoesntCrash) {
  NavigateToURL(browser(), GURL(chrome::kChromeUIMdSettingsURL));

  NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  WaitForLoadStop(browser()->tab_strip_model()->GetActiveWebContents());
}

// Catch lifetime issues in message handlers. There was previously a problem
// with PrefMember calling Init again after Destroy.
IN_PROC_BROWSER_TEST_F(MdSettingsUITest, ToggleJavaScript) {
  NavigateToURL(browser(), GURL(chrome::kChromeUIMdSettingsURL));

  const auto& handlers = *browser()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetWebUI()
                              ->GetHandlersForTesting();

  for (const std::unique_ptr<content::WebUIMessageHandler>& handler :
       handlers) {
    handler->AllowJavascriptForTesting();
    handler->DisallowJavascript();
    handler->AllowJavascriptForTesting();
  }
}
