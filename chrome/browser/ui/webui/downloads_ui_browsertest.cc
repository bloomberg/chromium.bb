// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_ui_browsertest.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/test_utils.h"

DownloadsUIBrowserTest::DownloadsUIBrowserTest() {}

DownloadsUIBrowserTest::~DownloadsUIBrowserTest() {}

void DownloadsUIBrowserTest::SetDeleteAllowed(bool allowed) {
  browser()->profile()->GetPrefs()->
      SetBoolean(prefs::kAllowDeletingBrowserHistory, allowed);
}

void DownloadsWebUIForSupervisedUsersTest::SetUpCommandLine(
    CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kSupervisedUserId, "asdf");
}
