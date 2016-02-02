// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/history_ui_browsertest.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"

HistoryUIBrowserTest::HistoryUIBrowserTest()
    : history_(NULL),
      baseline_time_(base::Time::Now().LocalMidnight()),
      nav_entry_id_(0) {
}

HistoryUIBrowserTest::~HistoryUIBrowserTest() {
}

void HistoryUIBrowserTest::SetUpOnMainThread() {
  WebUIBrowserTest::SetUpOnMainThread();

  history_ = HistoryServiceFactory::GetForProfile(
      browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
  ui_test_utils::WaitForHistoryToLoad(history_);
}

void HistoryUIBrowserTest::AddPageToHistory(
    int hour_offset, const std::string& url, const std::string& title) {
  // We need the ID scope and page ID so that the visit tracker can find it.
  const history::ContextID id_scope = reinterpret_cast<history::ContextID>(1);

  base::Time time = baseline_time_ + base::TimeDelta::FromHours(hour_offset);
  GURL gurl = GURL(url);
  history_->AddPage(gurl, time, id_scope, nav_entry_id_++, GURL(),
                    history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                    history::SOURCE_BROWSED, false);
  history_->SetPageTitle(gurl, base::UTF8ToUTF16(title));
}

void HistoryUIBrowserTest::SetDeleteAllowed(bool allowed) {
  browser()->profile()->GetPrefs()->
      SetBoolean(prefs::kAllowDeletingBrowserHistory, allowed);
}

void HistoryUIBrowserTest::ClearAcceptLanguages() {
  browser()->profile()->GetPrefs()->
      SetString(prefs::kAcceptLanguages, "");
}

