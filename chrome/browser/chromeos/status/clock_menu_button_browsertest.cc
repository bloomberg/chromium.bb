// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/clock_menu_button.h"

#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/system_access.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "unicode/calendar.h"
#include "unicode/timezone.h"

namespace chromeos {

class ClockMenuButtonTest : public InProcessBrowserTest {
 protected:
  ClockMenuButtonTest() : InProcessBrowserTest() {}
  virtual void SetUpInProcessBrowserTestFixture() {
    // This test requires actual libcros, but InProcessBrowserTest has set
    // to use stub, so reset it here.
    CrosLibrary::Get()->GetTestApi()->ResetUseStubImpl();
  }
  ClockMenuButton* GetClockMenuButton() {
    BrowserView* view = static_cast<BrowserView*>(browser()->window());
    return static_cast<StatusAreaView*>(view->
        GetViewByID(VIEW_ID_STATUS_AREA))->clock_view();
  }
};

IN_PROC_BROWSER_TEST_F(ClockMenuButtonTest, TimezoneTest) {
  ClockMenuButton* clock = GetClockMenuButton();
  ASSERT_TRUE(clock != NULL);

  // Update timezone and make sure clock text changes.
  scoped_ptr<icu::TimeZone> timezone_first(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8("Asia/Hong_Kong")));
  SystemAccess::GetInstance()->SetTimezone(*timezone_first);
  std::wstring text_before = clock->text();
  scoped_ptr<icu::TimeZone> timezone_second(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8("Pacific/Samoa")));
  SystemAccess::GetInstance()->SetTimezone(*timezone_second);
  std::wstring text_after = clock->text();
  EXPECT_NE(text_before, text_after);
}

}  // namespace chromeos
