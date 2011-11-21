// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/clock_menu_button.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "unicode/calendar.h"
#include "unicode/timezone.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"
#endif

namespace chromeos {

class ClockMenuButtonTest : public InProcessBrowserTest {
 protected:
  ClockMenuButtonTest() : InProcessBrowserTest() {}
  virtual void SetUpInProcessBrowserTestFixture() {
    // This test requires actual libcros, but InProcessBrowserTest has set
    // to use stub, so reset it here.
    CrosLibrary::Get()->GetTestApi()->ResetUseStubImpl();
  }
  const ClockMenuButton* GetClockMenuButton() {
    const views::View* parent = NULL;
#if defined(USE_AURA)
    parent = ChromeShellDelegate::instance()->GetStatusAreaForTest();
#else
    parent = static_cast<const BrowserView*>(browser()->window());
#endif
    return static_cast<const ClockMenuButton*>(
        parent->GetViewByID(VIEW_ID_STATUS_BUTTON_CLOCK));
  }
};

IN_PROC_BROWSER_TEST_F(ClockMenuButtonTest, TimezoneTest) {
  const ClockMenuButton* clock = GetClockMenuButton();
  ASSERT_TRUE(clock != NULL);

  // Update timezone and make sure clock text changes.
  scoped_ptr<icu::TimeZone> timezone_first(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8("Asia/Hong_Kong")));
  system::TimezoneSettings::GetInstance()->SetTimezone(*timezone_first);
  string16 text_before = clock->text();
  scoped_ptr<icu::TimeZone> timezone_second(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8("Pacific/Samoa")));
  system::TimezoneSettings::GetInstance()->SetTimezone(*timezone_second);
  string16 text_after = clock->text();
  EXPECT_NE(text_before, text_after);
}

}  // namespace chromeos
