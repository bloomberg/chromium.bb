// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/file_path.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"

class NewTabUITest : public UITest {
 public:
  NewTabUITest() {
    dom_automation_enabled_ = true;

    // Setup the DEFAULT_THEME profile (has fake history entries).
    set_template_user_data(UITest::ComputeTypicalUserDataSource(
        UITest::DEFAULT_THEME));
  }
};

TEST_F(NewTabUITest, NTPHasThumbnails) {
  // Switch to the "new tab" tab, which should be any new tab after the
  // first (the first is about:blank).
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);

  // Bring up a new tab page.
  window->ApplyAccelerator(IDC_NEW_TAB);
  WaitUntilTabCount(2);
  int load_time;
  ASSERT_TRUE(automation()->WaitForInitialNewTabUILoad(&load_time));

  // Blank thumbnails on the NTP have the class 'filler' applied to the div.
  // If all the thumbnails load, there should be no div's with 'filler'.
  scoped_refptr<TabProxy> tab = window->GetActiveTab();
  int filler_thumbnails_count = -1;
  const int kWaitDuration = 100;
  int wait_time = action_max_timeout_ms();
  while (wait_time > 0) {
    ASSERT_TRUE(tab->ExecuteAndExtractInt(L"",
        L"window.domAutomationController.send("
        L"document.getElementsByClassName('filler').length)",
        &filler_thumbnails_count));
    if (filler_thumbnails_count == 0)
      break;
    PlatformThread::Sleep(kWaitDuration);
    wait_time -= kWaitDuration;
  }
  EXPECT_EQ(0, filler_thumbnails_count);
}

TEST_F(NewTabUITest, UpdateUserPrefsVersion) {
  PrefService prefs((FilePath()));

  // Does the migration
  NewTabUI::RegisterUserPrefs(&prefs);

  ASSERT_EQ(NewTabUI::current_pref_version(),
            prefs.GetInteger(prefs::kNTPPrefVersion));

  // Reset the version
  prefs.ClearPref(prefs::kNTPPrefVersion);
  ASSERT_EQ(0, prefs.GetInteger(prefs::kNTPPrefVersion));

  bool migrated = NewTabUI::UpdateUserPrefsVersion(&prefs);
  ASSERT_TRUE(migrated);
  ASSERT_EQ(NewTabUI::current_pref_version(),
            prefs.GetInteger(prefs::kNTPPrefVersion));

  migrated = NewTabUI::UpdateUserPrefsVersion(&prefs);
  ASSERT_FALSE(migrated);
}
