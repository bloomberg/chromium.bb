// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/factory_ping_embargo_check.h"

#include <string>

#include "chromeos/system/statistics_provider.h"

namespace chromeos {
namespace system {

FactoryPingEmbargoState GetFactoryPingEmbargoState(
    StatisticsProvider* statistics_provider) {
  std::string rlz_embargo_end_date;
  if (!statistics_provider->GetMachineStatistic(kRlzEmbargoEndDateKey,
                                                &rlz_embargo_end_date)) {
    // |rlz_embargo_end_date| only exists on new devices that have not yet
    // launched.
    return FactoryPingEmbargoState::kMissingOrMalformed;
  }
  base::Time parsed_time;
  if (!base::Time::FromUTCString(rlz_embargo_end_date.c_str(), &parsed_time)) {
    LOG(ERROR) << "|rlz_embargo_end_date| exists but cannot be parsed.";
    return FactoryPingEmbargoState::kMissingOrMalformed;
  }

  if (parsed_time - base::Time::Now() >=
      kRlzEmbargoEndDateGarbageDateThreshold) {
    // If |rlz_embargo_end_date| is more than this many days in the future,
    // ignore it. Because it indicates that the device is not connected to an
    // ntp server in the factory, and its internal clock could be off when the
    // date is written.
    // TODO(pmarko): UMA stat for how often this happens
    // (https://crbug.com/839353).
    return FactoryPingEmbargoState::kInvalid;
  }

  return base::Time::Now() > parsed_time ? FactoryPingEmbargoState::kPassed
                                         : FactoryPingEmbargoState::kNotPassed;
}

}  // namespace system
}  // namespace chromeos
