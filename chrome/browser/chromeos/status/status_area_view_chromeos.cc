// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/status_area_view_chromeos.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/status/accessibility_menu_button.h"
#include "chrome/browser/chromeos/status/caps_lock_menu_button.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/input_method_menu_button.h"
#include "chrome/browser/chromeos/status/memory_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/power_menu_button.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/common/chrome_switches.h"

namespace chromeos {

StatusAreaViewChromeos::StatusAreaViewChromeos() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  system::TimezoneSettings::GetInstance()->AddObserver(this);
}

StatusAreaViewChromeos::~StatusAreaViewChromeos() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void StatusAreaViewChromeos::Init(StatusAreaButton::Delegate* delegate,
                                  ScreenMode screen_mode) {
  AddChromeosButtons(this, delegate, screen_mode, NULL);
}

void StatusAreaViewChromeos::SystemResumed() {
  UpdateClockText();
}

void StatusAreaViewChromeos::TimezoneChanged(const icu::TimeZone& timezone) {
  UpdateClockText();
}

void StatusAreaViewChromeos::UpdateClockText() {
  ClockMenuButton* clock_button =
      static_cast<ClockMenuButton*>(GetViewByID(VIEW_ID_STATUS_BUTTON_CLOCK));
  if (clock_button)
    clock_button->UpdateText();
}

void StatusAreaViewChromeos::SetDefaultUse24HourClock(bool use_24hour_clock) {
  ClockMenuButton* clock_button =
      static_cast<ClockMenuButton*>(GetViewByID(VIEW_ID_STATUS_BUTTON_CLOCK));
  if (clock_button)
    clock_button->SetDefaultUse24HourClock(use_24hour_clock);
}

// static
void StatusAreaViewChromeos::AddChromeosButtons(
    StatusAreaView* status_area,
    StatusAreaButton::Delegate* delegate,
    ScreenMode screen_mode,
    ClockMenuButton** clock_button) {
  const bool border = true;
  const bool no_border = false;

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kMemoryWidget))
    status_area->AddButton(new MemoryMenuButton(delegate), no_border);

  status_area->AddButton(
      new AccessibilityMenuButton(delegate, screen_mode), border);
  status_area->AddButton(new CapsLockMenuButton(delegate), border);
  ClockMenuButton* clock = new ClockMenuButton(delegate);
  status_area->AddButton(clock, border);
  if (clock_button)
    *clock_button = clock;

  status_area->AddButton(
      new InputMethodMenuButton(delegate, screen_mode), no_border);
  status_area->AddButton(
      new NetworkMenuButton(delegate, screen_mode), no_border);
  status_area->AddButton(new PowerMenuButton(delegate), no_border);
}

}  // namespace chromeos
