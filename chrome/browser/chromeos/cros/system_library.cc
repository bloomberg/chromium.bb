// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/system_library.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

SystemLibraryImpl::SystemLibraryImpl() {
  std::string id = "US/Pacific";
  if (CrosLibrary::Get()->EnsureLoaded()) {
    std::string timezone_id = chromeos::GetTimezoneID();
    if (timezone_id.empty()) {
      LOG(ERROR) << "Got an empty string for timezone, default to " << id;
    } else {
      id = timezone_id;
    }
  }
  icu::TimeZone* timezone =
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(id));
  timezone_.reset(timezone);
  icu::TimeZone::setDefault(*timezone);
  LOG(INFO) << "Timezone is " << id;
}

void SystemLibraryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SystemLibraryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const icu::TimeZone& SystemLibraryImpl::GetTimezone() {
  return *timezone_.get();
}

void SystemLibraryImpl::SetTimezone(const icu::TimeZone* timezone) {
  timezone_.reset(timezone->clone());
  if (CrosLibrary::Get()->EnsureLoaded()) {
    icu::UnicodeString unicode;
    timezone->getID(unicode);
    std::string id;
    UTF16ToUTF8(unicode.getBuffer(), unicode.length(), &id);
    LOG(INFO) << "Setting timezone to " << id;
    chromeos::SetTimezoneID(id);
  }
  icu::TimeZone::setDefault(*timezone);
  FOR_EACH_OBSERVER(Observer, observers_, TimezoneChanged(*timezone));
}

}  // namespace chromeos
