// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_POWER_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_POWER_LIBRARY_H_
#pragma once

#include "chrome/browser/chromeos/cros/power_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockPowerLibrary : public PowerLibrary {
 public:
  MockPowerLibrary() {}
  virtual ~MockPowerLibrary() {}
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));

  MOCK_CONST_METHOD0(line_power_on, bool(void));
  MOCK_CONST_METHOD0(battery_fully_charged, bool(void));
  MOCK_CONST_METHOD0(battery_percentage, double(void));
  MOCK_CONST_METHOD0(battery_is_present, bool(void));
  MOCK_CONST_METHOD0(battery_time_to_empty, base::TimeDelta(void));
  MOCK_CONST_METHOD0(battery_time_to_full, base::TimeDelta(void));

  MOCK_METHOD1(EnableScreenLock, void(bool));
  MOCK_METHOD0(RequestRestart, void(void));
  MOCK_METHOD0(RequestShutdown, void(void));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_POWER_LIBRARY_H_
