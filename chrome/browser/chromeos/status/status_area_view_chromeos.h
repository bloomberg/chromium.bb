// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_CHROMEOS_H_
#pragma once

#include "chrome/browser/chromeos/status/status_area_view.h"

#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/chromeos/view_ids.h"

class ClockMenuButton;

namespace chromeos {

class StatusAreaViewChromeos : public StatusAreaView,
                               public PowerLibrary::Observer,
                               public system::TimezoneSettings::Observer {
 public:
  // The type of screen the host window is on.
  enum ScreenMode {
    LOGIN_MODE_VIEWS,    // The host is for the views-based OOBE/login screens.
    LOGIN_MODE_WEBUI,    // The host is for the WebUI OOBE/login screens.
    BROWSER_MODE,        // The host is for browser.
    SCREEN_LOCKER_MODE,  // The host is for screen locker.
  };

  explicit StatusAreaViewChromeos();
  virtual ~StatusAreaViewChromeos();

  void Init(StatusAreaButton::Delegate* delegate, ScreenMode screen_mode);

  // PowerLibrary::Observer:
  virtual void SystemResumed() OVERRIDE;

  // TimezoneSettings::Observer:
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE;

  // Sets default use 24hour clock mode.
  void SetDefaultUse24HourClock(bool use_24hour_clock);

  // Convenience function to add buttons to a status area for ChromeOS.
  // |clock_button| (if non-NULL) is set to the ClockMenuButton that is created
  // by this method.
  static void AddChromeosButtons(StatusAreaView* status_area,
                                 StatusAreaButton::Delegate* delegate,
                                 ScreenMode screen_mode,
                                 ClockMenuButton** clock_button);

 private:
  void UpdateClockText();

  DISALLOW_COPY_AND_ASSIGN(StatusAreaViewChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_CHROMEOS_H_
