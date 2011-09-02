// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/clock_menu_button.h"

#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "views/controls/menu/menu_runner.h"
#include "views/widget/widget.h"
#include "unicode/datefmt.h"

namespace {

// views::MenuItemView item ids
enum ClockMenuItem {
  CLOCK_DISPLAY_ITEM,
  CLOCK_OPEN_OPTIONS_ITEM
};

}  // namespace

namespace chromeos {

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

ClockMenuButton::ClockMenuButton(StatusAreaHost* host)
    : StatusAreaButton(host, this) {
  // Add as TimezoneSettings observer. We update the clock if timezone changes.
  system::TimezoneSettings::GetInstance()->AddObserver(this);
  CrosLibrary::Get()->GetPowerLibrary()->AddObserver(this);
  // Start monitoring the kUse24HourClock preference.
  if (host->GetProfile()) {  // This can be NULL in the login screen.
    registrar_.Init(host->GetProfile()->GetPrefs());
    registrar_.Add(prefs::kUse24HourClock, this);
  }

  UpdateTextAndSetNextTimer();
}

ClockMenuButton::~ClockMenuButton() {
  timer_.Stop();
  CrosLibrary::Get()->GetPowerLibrary()->RemoveObserver(this);
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
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

  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(seconds_left), this,
               &ClockMenuButton::UpdateTextAndSetNextTimer);
}

void ClockMenuButton::UpdateText() {
  base::Time time(base::Time::Now());
  // If the profie is present, check the use 24-hour clock preference.
  const bool use_24hour_clock =
      host_->GetProfile() &&
      host_->GetProfile()->GetPrefs()->GetBoolean(prefs::kUse24HourClock);
  SetText(UTF16ToWide(base::TimeFormatTimeOfDayWithHourClockType(
      time,
      use_24hour_clock ? base::k24HourClock : base::k12HourClock,
      base::kDropAmPm)));
  SetTooltipText(UTF16ToWide(base::TimeFormatFriendlyDateAndTime(time)));
  SetAccessibleName(base::TimeFormatFriendlyDateAndTime(time));
  SchedulePaint();
}

// ClockMenuButton, NotificationObserver implementation:

void ClockMenuButton::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* pref_name = Details<std::string>(details).ptr();
    if (*pref_name == prefs::kUse24HourClock) {
      UpdateText();
    }
  }
}

// ClockMenuButton, views::MenuDelegate implementation:
std::wstring ClockMenuButton::GetLabel(int id) const {
  DCHECK_EQ(CLOCK_DISPLAY_ITEM, id);
  const string16 label = base::TimeFormatFriendlyDate(base::Time::Now());
  return UTF16ToWide(label);
}

bool ClockMenuButton::IsCommandEnabled(int id) const {
  DCHECK(id == CLOCK_DISPLAY_ITEM || id == CLOCK_OPEN_OPTIONS_ITEM);
  return id == CLOCK_OPEN_OPTIONS_ITEM;
}

void ClockMenuButton::ExecuteCommand(int id) {
  DCHECK_EQ(CLOCK_OPEN_OPTIONS_ITEM, id);
  host_->OpenButtonOptions(this);
}

// ClockMenuButton, PowerLibrary::Observer implementation:

void ClockMenuButton::SystemResumed() {
  UpdateText();
}

// ClockMenuButton, SystemLibrary::Observer implementation:

void ClockMenuButton::TimezoneChanged(const icu::TimeZone& timezone) {
  UpdateText();
}

int ClockMenuButton::horizontal_padding() {
  return 3;
}

// ClockMenuButton, views::ViewMenuDelegate implementation:

void ClockMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  // View passed in must be a views::MenuButton, i.e. the ClockMenuButton.
  DCHECK_EQ(source, this);

  EnsureMenu();

  gfx::Point screen_location;
  views::View::ConvertPointToScreen(source, &screen_location);
  gfx::Rect bounds(screen_location, source->size());
  if (menu_runner_->RunMenuAt(
          source->GetWidget()->GetTopLevelWidget(), this, bounds,
          views::MenuItemView::TOPRIGHT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
}

// ClockMenuButton, views::View implementation:

void ClockMenuButton::OnLocaleChanged() {
  UpdateText();
}

void ClockMenuButton::EnsureMenu() {
  if (menu_runner_.get())
    return;

  views::MenuItemView* menu = new views::MenuItemView(this);
  // menu_runner_ takes ownership of menu.
  menu_runner_.reset(new views::MenuRunner(menu));

  // Text for this item will be set by GetLabel().
  menu->AppendDelegateMenuItem(CLOCK_DISPLAY_ITEM);

  // If options dialog is unavailable, don't show a separator and configure
  // menu item.
  if (host_->ShouldOpenButtonOptions(this)) {
    menu->AppendSeparator();

    const string16 clock_open_options_label =
        l10n_util::GetStringUTF16(IDS_STATUSBAR_CLOCK_OPEN_OPTIONS_DIALOG);
    menu->AppendMenuItemWithLabel(
        CLOCK_OPEN_OPTIONS_ITEM,
        UTF16ToWide(clock_open_options_label));
  }
}

}  // namespace chromeos
