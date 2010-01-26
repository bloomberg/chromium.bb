// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/clock_menu_button.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "unicode/calendar.h"

namespace chromeos {

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

ClockMenuButton::ClockMenuButton(Browser* browser)
    : MenuButton(NULL, std::wstring(), this, false),
      clock_menu_(this),
      browser_(browser) {
  set_border(NULL);
  SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD));
  SetEnabledColor(SK_ColorWHITE);
  SetShowHighlighted(false);
  // Set initial text to make sure that the text button wide enough.
  std::wstring zero = ASCIIToWide("00");
  SetText(l10n_util::GetStringF(IDS_STATUSBAR_CLOCK_SHORT_TIME_AM, zero, zero));
  SetText(l10n_util::GetStringF(IDS_STATUSBAR_CLOCK_SHORT_TIME_PM, zero, zero));
  set_alignment(TextButton::ALIGN_RIGHT);
  UpdateTextAndSetNextTimer();
  // Init member prefs so we can update the controls if prefs change.
  timezone_.Init(prefs::kTimeZone, browser_->profile()->GetPrefs(), this);
}

void ClockMenuButton::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    const std::wstring* pref_name = Details<std::wstring>(details).ptr();
    if (!pref_name || *pref_name == prefs::kTimeZone)
      UpdateText();
  }
}

void ClockMenuButton::UpdateTextAndSetNextTimer() {
  UpdateText();

  // Try to set the timer to go off at the next change of the minute. We don't
  // want to have the timer go off more than necessary since that will cause
  // the CPU to wake up and consume power.
  base::Time now = base::Time::Now();
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);

  // Often this will be called at minute boundaries, and we'll actually want
  // 60 seconds from now.
  int seconds_left = 60 - exploded.second;
  if (seconds_left == 0)
    seconds_left = 60;

  // Make sure that the timer fires on the next minute. Without this, if it is
  // called just a teeny bit early, then it will skip the next minute.
  seconds_left += kTimerSlopSeconds;

  timer_.Start(base::TimeDelta::FromSeconds(seconds_left), this,
               &ClockMenuButton::UpdateTextAndSetNextTimer);
}

void ClockMenuButton::UpdateText() {
  // Use icu::Calendar because the correct timezone is set on icu::TimeZone's
  // default timezone.
  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Calendar> cal(icu::Calendar::createInstance(error));
  if (!cal.get())
    return;

  int hour = cal->get(UCAL_HOUR, error);
  int minute = cal->get(UCAL_MINUTE, error);
  int ampm = cal->get(UCAL_AM_PM, error);

  if (hour == 0)
    hour = 12;
  std::wstring hour_str = IntToWString(hour);
  std::wstring min_str = IntToWString(minute);
  // Append a "0" before the minute if it's only a single digit.
  if (minute < 10)
    min_str = IntToWString(0) + min_str;
  int msg = (ampm == UCAL_AM) ? IDS_STATUSBAR_CLOCK_SHORT_TIME_AM :
                                IDS_STATUSBAR_CLOCK_SHORT_TIME_PM;

  std::wstring time_string = l10n_util::GetStringF(msg, hour_str, min_str);

  SetText(time_string);
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, menus::MenuModel implementation:

int ClockMenuButton::GetItemCount() const {
  return 3;
}

menus::MenuModel::ItemType ClockMenuButton::GetTypeAt(int index) const {
  // There's a separator between the current date and the menu item to open
  // the options menu.
  return index == 1 ? menus::MenuModel::TYPE_SEPARATOR:
                      menus::MenuModel::TYPE_COMMAND;
}

string16 ClockMenuButton::GetLabelAt(int index) const {
  if (index == 0)
    return WideToUTF16(base::TimeFormatFriendlyDate(base::Time::Now()));
  return l10n_util::GetStringUTF16(IDS_STATUSBAR_CLOCK_OPEN_OPTIONS_DIALOG);
}

bool ClockMenuButton::IsEnabledAt(int index) const {
  // The 1st item is the current date, which is disabled.
  return index != 0;
}

void ClockMenuButton::ActivatedAt(int index) {
  browser_->OpenSystemOptionsDialog();
}

////////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, views::ViewMenuDelegate implementation:

void ClockMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  clock_menu_.Rebuild();
  clock_menu_.UpdateStates();
  clock_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

}  // namespace chromeos
