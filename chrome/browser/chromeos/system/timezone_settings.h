// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_SETTINGS_H_

#include <vector>

#include "base/string16.h"
#include "third_party/icu/public/i18n/unicode/timezone.h"

namespace chromeos {
namespace system {

// This interface provides access to Chrome OS timezone settings.
class TimezoneSettings {
 public:
  class Observer {
   public:
    // Called when the timezone has changed. |timezone| is non-null.
    virtual void TimezoneChanged(const icu::TimeZone& timezone) = 0;
   protected:
    virtual ~Observer();
  };

  static TimezoneSettings* GetInstance();

  // Returns the current timezone as an icu::Timezone object.
  virtual const icu::TimeZone& GetTimezone() = 0;
  virtual string16 GetCurrentTimezoneID() = 0;

  // Sets the current timezone and notifies all Observers.
  virtual void SetTimezone(const icu::TimeZone& timezone) = 0;
  virtual void SetTimezoneFromID(const string16& timezone_id) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual const std::vector<icu::TimeZone*>& GetTimezoneList() const = 0;

  // Gets timezone ID which is also used as timezone pref value.
  static string16 GetTimezoneID(const icu::TimeZone& timezone);

 protected:
  virtual ~TimezoneSettings() {}
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_SETTINGS_H_
