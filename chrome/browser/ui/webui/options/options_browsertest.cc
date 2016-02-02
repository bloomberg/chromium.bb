// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_browsertest.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/prefs/pref_service.h"
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

  web_ui()->RegisterMessageCallback(
      "optionsTestSetPref", base::Bind(&OptionsBrowserTest::HandleSetPref,
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
      "OptionsWebUIExtendedTest.verifyHistoryCallback", history);
}

void OptionsBrowserTest::ClearPref(const char* path) {
  browser()->profile()->GetPrefs()->ClearPref(path);
}

void OptionsBrowserTest::HandleSetPref(const base::ListValue* args) {
  ASSERT_EQ(2u, args->GetSize());

  std::string pref_name;
  ASSERT_TRUE(args->GetString(0, &pref_name));
  const base::Value* pref_value;
  ASSERT_TRUE(args->Get(1, &pref_value));

  browser()->profile()->GetPrefs()->Set(pref_name.c_str(), *pref_value);
}

content::WebUIMessageHandler* OptionsBrowserTest::GetMockMessageHandler() {
  return this;
}
