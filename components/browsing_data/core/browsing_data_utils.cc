// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_utils.h"

namespace browsing_data {

base::Time CalculateBeginDeleteTime(TimePeriod time_period) {
  base::TimeDelta diff;
  base::Time delete_begin_time = base::Time::Now();
  switch (time_period) {
    case LAST_HOUR:
      diff = base::TimeDelta::FromHours(1);
      break;
    case LAST_DAY:
      diff = base::TimeDelta::FromHours(24);
      break;
    case LAST_WEEK:
      diff = base::TimeDelta::FromHours(7 * 24);
      break;
    case FOUR_WEEKS:
      diff = base::TimeDelta::FromHours(4 * 7 * 24);
      break;
    case ALL_TIME:
      delete_begin_time = base::Time();
      break;
  }
  return delete_begin_time - diff;
}

}  // namespace browsing_data
