// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SETTINGS_TIMEZONE_SETTINGS_HELPER_H_
#define CHROMEOS_SETTINGS_TIMEZONE_SETTINGS_HELPER_H_

#include <memory>
#include <vector>

#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace chromeos {
namespace system {

// Return a timezone in the list matching |timezone| in terms of
// id and canonical id. If both fail, return a timezone with
// the same rules. Otherwise, return null.
const icu::TimeZone* GetKnownTimezoneOrNull(
    const icu::TimeZone& timezone,
    const std::vector<std::unique_ptr<icu::TimeZone>>& timezone_list);

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_SETTINGS_TIMEZONE_SETTINGS_HELPER_H_
