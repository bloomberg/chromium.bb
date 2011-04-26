// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MOCK_SYSTEM_ACCESS_H_
#define CHROME_BROWSER_CHROMEOS_MOCK_SYSTEM_ACCESS_H_
#pragma once

#include "chrome/browser/chromeos/system_access.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSystemAccess : public SystemAccess {
 public:
  MockSystemAccess() {}

  MOCK_METHOD0(GetTimezone, const icu::TimeZone&());
  MOCK_METHOD1(SetTimezone, void(const icu::TimeZone& timezone));
  MOCK_METHOD2(GetMachineStatistic, bool(const std::string& name,
                                         std::string* result));
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemAccess);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MOCK_SYSTEM_ACCESS_H_
