// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_ui_browsertest.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/downloads_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_utils.h"

DownloadsUIBrowserTest::DownloadsUIBrowserTest() {}

DownloadsUIBrowserTest::~DownloadsUIBrowserTest() {}

void DownloadsUIBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  WebUIBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kDisableMaterialDesignDownloads);
  ASSERT_FALSE(MdDownloadsEnabled());
}

void DownloadsUIBrowserTest::SetDeleteAllowed(bool allowed) {
  browser()->profile()->GetPrefs()->
      SetBoolean(prefs::kAllowDeletingBrowserHistory, allowed);
}
