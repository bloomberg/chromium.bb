// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_MOCK_STATISTICS_PROVIDER_H_
#define CHROMEOS_SYSTEM_MOCK_STATISTICS_PROVIDER_H_

#include "base/basictypes.h"
#include "chromeos/system/statistics_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace system {

class CHROMEOS_EXPORT MockStatisticsProvider : public StatisticsProvider {
 public:
  MockStatisticsProvider();
  virtual ~MockStatisticsProvider();

  MOCK_METHOD2(StartLoadingMachineStatistics, void(
      const scoped_refptr<base::TaskRunner>&,
      bool));
  MOCK_METHOD2(GetMachineStatistic, bool(const std::string& name,
                                         std::string* result));
  MOCK_METHOD2(GetMachineFlag, bool(const std::string& name,
                                    bool* result));
  MOCK_METHOD0(Shutdown, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStatisticsProvider);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_MOCK_STATISTICS_PROVIDER_H_
