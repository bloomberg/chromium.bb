// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/fake_system_library.h"

namespace chromeos {

FakeSystemLibrary::FakeSystemLibrary() {
  std::string id = "US/Pacific";
  icu::TimeZone* timezone =
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(id));
  timezone_.reset(timezone);
}

void FakeSystemLibrary::AddObserver(Observer* observer) {
}

void FakeSystemLibrary::RemoveObserver(Observer* observer) {
}

const icu::TimeZone& FakeSystemLibrary::GetTimezone() {
  return *timezone_.get();
}

void FakeSystemLibrary::SetTimezone(const icu::TimeZone* timezone) {
}

}  // namespace chromeos
