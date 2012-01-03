// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_SETTINGS_H_
#pragma once

#include <string>

#include "chrome/browser/cancelable_request.h"
#include "unicode/timezone.h"

namespace chromeos {
namespace system {

// This interface provides access to Chrome OS timezone settings.
class TimezoneSettings : public CancelableRequestProvider {
 public:
  class Observer {
   public:
    // Called when the timezone has changed. |timezone| is non-null.
    virtual void TimezoneChanged(const icu::TimeZone& timezone) = 0;
  };

  static TimezoneSettings* GetInstance();

  // Returns the current timezone as an icu::Timezone object.
  virtual const icu::TimeZone& GetTimezone() = 0;

  // Sets the current timezone. |timezone| must be non-null.
  virtual void SetTimezone(const icu::TimeZone& timezone) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  virtual ~TimezoneSettings() {}
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_SETTINGS_H_
