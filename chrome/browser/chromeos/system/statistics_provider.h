// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_
#pragma once

#include <string>

namespace chromeos {
namespace system {

// This interface provides access to Chrome OS statistics.
class StatisticsProvider {
 public:
  // Retrieve the named machine statistic (e.g. "hardware_class").
  // This does not update the statistcs. If the |name| is not set, |result|
  // preserves old value.
  virtual bool GetMachineStatistic(const std::string& name,
                                   std::string* result) = 0;

  static StatisticsProvider* GetInstance();

 protected:
  virtual ~StatisticsProvider() {}
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_STATISTICS_PROVIDER_H_
