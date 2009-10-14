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
#include "grit/generated_resources.h"

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

ClockMenuButton::ClockMenuButton()
    : MenuButton(NULL, std::wstring(), this, false),
      clock_menu_(this) {
  SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD));
  SetEnabledColor(SK_ColorWHITE);
  SetShowHighlighted(false);
  // Set initial text to make sure that the text button wide enough.
  std::wstring zero = ASCIIToWide("00");
  SetText(l10n_util::GetStringF(IDS_STATUSBAR_CLOCK_SHORT_TIME_AM, zero, zero));
  SetText(l10n_util::GetStringF(IDS_STATUSBAR_CLOCK_SHORT_TIME_PM, zero, zero));
  set_alignment(TextButton::ALIGN_RIGHT);
  UpdateText();
}

void ClockMenuButton::SetNextTimer() {
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
               &ClockMenuButton::UpdateText);
}

void ClockMenuButton::UpdateText() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  int hour = now.hour % 12;
  if (hour == 0)
    hour = 12;

  std::wstring hour_str = IntToWString(hour);
  std::wstring min_str = IntToWString(now.minute);
  // Append a "0" before the minute if it's only a single digit.
  if (now.minute < 10)
    min_str = IntToWString(0) + min_str;
  int msg = now.hour < 12 ? IDS_STATUSBAR_CLOCK_SHORT_TIME_AM :
                            IDS_STATUSBAR_CLOCK_SHORT_TIME_PM;

  std::wstring time_string = l10n_util::GetStringF(msg, hour_str, min_str);

  SetText(time_string);
  SchedulePaint();
  SetNextTimer();
}

////////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, views::Menu2Model implementation:

int ClockMenuButton::GetItemCount() const {
  return 1;
}

views::Menu2Model::ItemType ClockMenuButton::GetTypeAt(int index) const {
  return views::Menu2Model::TYPE_COMMAND;
}

string16 ClockMenuButton::GetLabelAt(int index) const {
  return WideToUTF16(base::TimeFormatFriendlyDate(base::Time::Now()));
}

////////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, views::ViewMenuDelegate implementation:

void ClockMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  clock_menu_.Rebuild();
  clock_menu_.UpdateStates();
  clock_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}
