// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_browsertest.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

using content::NavigationController;
using content::NavigationEntry;
using content::WebUIMessageHandler;

OptionsBrowserTest::OptionsBrowserTest() {
}

OptionsBrowserTest::~OptionsBrowserTest() {
}

void OptionsBrowserTest::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "optionsTestReportHistory", base::Bind(&OptionsBrowserTest::ReportHistory,
                                             base::Unretained(this)));
}

// Includes the current entry.
void OptionsBrowserTest::ReportHistory(const base::ListValue* list_value) {
  const NavigationController& controller =
      browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  base::ListValue history;
  const int current = controller.GetCurrentEntryIndex();
  for (int i = 0; i <= current; ++i) {
    GURL url = controller.GetEntryAtIndex(i)->GetVirtualURL();
    history.Append(new base::StringValue(url.spec()));
  }
  web_ui()->CallJavascriptFunction(
      "OptionsWebUINavigationTest.verifyHistoryCallback", history);
}

content::WebUIMessageHandler* OptionsBrowserTest::GetMockMessageHandler() {
  return this;
}
