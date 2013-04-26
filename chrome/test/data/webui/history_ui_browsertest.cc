// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/history_ui_browsertest.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"

HistoryUIBrowserTest::HistoryUIBrowserTest() {
}

HistoryUIBrowserTest::~HistoryUIBrowserTest() {
}

void HistoryUIBrowserTest::SetDeleteAllowed(bool allowed) {
  browser()->profile()->GetPrefs()->
      SetBoolean(prefs::kAllowDeletingBrowserHistory, allowed);
}
