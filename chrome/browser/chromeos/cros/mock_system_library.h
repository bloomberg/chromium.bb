// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_SYSTEM_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_SYSTEM_LIBRARY_H_

#include "chrome/browser/chromeos/cros/system_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSystemLibrary : public SystemLibrary {
 public:
  MockSystemLibrary() {}
  virtual ~MockSystemLibrary() {}
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD0(GetTimezone, const icu::TimeZone&());
  MOCK_METHOD1(SetTimezone, void(const icu::TimeZone*));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_SYSTEM_LIBRARY_H_

