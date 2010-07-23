// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_SYSTEM_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_SYSTEM_LIBRARY_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/system_library.h"
#include "unicode/timezone.h"

namespace chromeos {

class FakeSystemLibrary : public SystemLibrary {
 public:
  FakeSystemLibrary() {
    std::string id = "US/Pacific";
    icu::TimeZone* timezone =
        icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(id));
    timezone_.reset(timezone);
  }
  virtual ~FakeSystemLibrary() {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual const icu::TimeZone& GetTimezone() {
    return *timezone_.get();
  }
  virtual void SetTimezone(const icu::TimeZone*) {}

 private:
  scoped_ptr<icu::TimeZone> timezone_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_SYSTEM_LIBRARY_H_
