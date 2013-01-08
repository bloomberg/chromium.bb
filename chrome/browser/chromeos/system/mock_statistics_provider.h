// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_MOCK_STATISTICS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_MOCK_STATISTICS_PROVIDER_H_

#include "base/basictypes.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace system {

class MockStatisticsProvider : public system::StatisticsProvider {
 public:
  MockStatisticsProvider();
  virtual ~MockStatisticsProvider();

  MOCK_METHOD0(Init, void());
  MOCK_METHOD0(StartLoadingMachineStatistics, void());
  MOCK_METHOD2(GetMachineStatistic, bool(const std::string& name,
                                         std::string* result));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStatisticsProvider);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_MOCK_STATISTICS_PROVIDER_H_
