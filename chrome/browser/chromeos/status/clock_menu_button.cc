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
#include "chrome/common/pref_names.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "unicode/datefmt.h"

namespace chromeos {

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

ClockMenuButton::ClockMenuButton(StatusAreaHost* host)
    : StatusAreaButton(host, this) {
  // Add as SystemAccess observer. We update the clock if timezone changes.
  SystemAccess::GetInstance()->AddObserver(this);
  CrosLibrary::Get()->GetPowerLibrary()->AddObserver(this);
  // Start monitoring the kUse24HourClock preference.
  if (host->GetProfile()) {  // This can be NULL in the login screen.
    registrar_.Init(host->GetProfile()->GetPrefs());
    registrar_.Add(prefs::kUse24HourClock, this);
  }

  UpdateTextAndSetNextTimer();
}

ClockMenuButton::~ClockMenuButton() {
  CrosLibrary::Get()->GetPowerLibrary()->RemoveObserver(this);
  SystemAccess::GetInstance()->RemoveObserver(this);
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
  base::Time time(base::Time::Now());
  // If the profie is present, check the use 24-hour clock preference.
  const bool use_24hour_clock =
      host_->GetProfile() &&
      host_->GetProfile()->GetPrefs()->GetBoolean(prefs::kUse24HourClock);
  if (use_24hour_clock) {
    SetText(UTF16ToWide(base::TimeFormatTimeOfDayWithHourClockType(
        time, base::k24HourClock)));
  } else {
    // Remove the am/pm field if it's present.
    scoped_ptr<icu::DateFormat> formatter(
        icu::DateFormat::createTimeInstance(icu::DateFormat::kShort));
    icu::UnicodeString time_string;
    icu::FieldPosition ampm_field(icu::DateFormat::kAmPmField);
    formatter->format(
        static_cast<UDate>(time.ToDoubleT() * 1000), time_string, ampm_field);
    int ampm_length = ampm_field.getEndIndex() - ampm_field.getBeginIndex();
    if (ampm_length) {
      int begin = ampm_field.getBeginIndex();
      // Doesn't include any spacing before the field.
      if (begin)
        begin--;
      time_string.removeBetween(begin, ampm_field.getEndIndex());
    }
    string16 time_string16 =
        string16(time_string.getBuffer(),
                 static_cast<size_t>(time_string.length()));
    SetText(UTF16ToWide(time_string16));
  }
  SetTooltipText(UTF16ToWide(base::TimeFormatShortDate(time)));
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, NotificationObserver implementation:

void ClockMenuButton::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::string* pref_name = Details<std::string>(details).ptr();
    if (*pref_name == prefs::kUse24HourClock) {
      UpdateText();
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, ui::MenuModel implementation:

int ClockMenuButton::GetItemCount() const {
  // If options dialog is unavailable, don't count a separator and configure
  // menu item.
  return host_->ShouldOpenButtonOptions(this) ? 3 : 1;
}

ui::MenuModel::ItemType ClockMenuButton::GetTypeAt(int index) const {
  // There's a separator between the current date and the menu item to open
  // the options menu.
  return index == 1 ? ui::MenuModel::TYPE_SEPARATOR:
                      ui::MenuModel::TYPE_COMMAND;
}

string16 ClockMenuButton::GetLabelAt(int index) const {
  if (index == 0)
    return base::TimeFormatFriendlyDate(base::Time::Now());
  return l10n_util::GetStringUTF16(IDS_STATUSBAR_CLOCK_OPEN_OPTIONS_DIALOG);
}

bool ClockMenuButton::IsEnabledAt(int index) const {
  // The 1st item is the current date, which is disabled.
  return index != 0;
}

void ClockMenuButton::ActivatedAt(int index) {
  host_->OpenButtonOptions(this);
}

///////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, PowerLibrary::Observer implementation:

void ClockMenuButton::SystemResumed() {
  UpdateText();
}

///////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, SystemAccess::Observer implementation:

void ClockMenuButton::TimezoneChanged(const icu::TimeZone& timezone) {
  UpdateText();
}

////////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, views::ViewMenuDelegate implementation:

void ClockMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  if (!clock_menu_.get())
    clock_menu_.reset(new views::Menu2(this));
  else
    clock_menu_->Rebuild();
  clock_menu_->UpdateStates();
  clock_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// ClockMenuButton, views::View implementation:

void ClockMenuButton::OnLocaleChanged() {
  UpdateText();
}

}  // namespace chromeos
