// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_UTIL_H_

#include <memory>

#include "base/strings/string16.h"

namespace base {
class ListValue;
}

namespace chromeos {

struct TimeZoneResponseData;

namespace system {

// Gets the current timezone's display name.
base::string16 GetCurrentTimezoneName();

// Creates a list of pairs of each timezone's ID and name.
std::unique_ptr<base::ListValue> GetTimezoneList();

// Returns true if device is managed and has SystemTimezonePolicy set.
bool HasSystemTimezonePolicy();

// Apply TimeZone update from TimeZoneProvider.
void ApplyTimeZone(const TimeZoneResponseData* timezone);

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_UTIL_H_
