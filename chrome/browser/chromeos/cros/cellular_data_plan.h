// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CELLULAR_DATA_PLAN_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CELLULAR_DATA_PLAN_H_

#include <string>

#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "base/time.h"

namespace chromeos {

// Enum to describe type of a plan.
enum CellularDataPlanType {
  CELLULAR_DATA_PLAN_UNKNOWN = 0,
  CELLULAR_DATA_PLAN_UNLIMITED = 1,
  CELLULAR_DATA_PLAN_METERED_PAID = 2,
  CELLULAR_DATA_PLAN_METERED_BASE = 3
};

// Cellular network is considered low data when less than 60 minues.
extern const int kCellularDataLowSecs;

// Cellular network is considered low data when less than 30 minues.
extern const int kCellularDataVeryLowSecs;

// Cellular network is considered low data when less than 100MB.
extern const int kCellularDataLowBytes;

// Cellular network is considered very low data when less than 50MB.
extern const int kCellularDataVeryLowBytes;

class CellularDataPlan {
 public:
  CellularDataPlan();
  ~CellularDataPlan();

  // Formats cellular plan description.
  string16 GetPlanDesciption() const;
  // Evaluates cellular plans status and returns warning string if it is near
  // expiration.
  string16 GetRemainingWarning() const;
  // Formats remaining plan data description.
  string16 GetDataRemainingDesciption() const;
  // Formats plan expiration description.
  string16 GetPlanExpiration() const;
  // Formats plan usage info.
  string16 GetUsageInfo() const;
  // Returns a unique string for this plan that can be used for comparisons.
  std::string GetUniqueIdentifier() const;
  base::TimeDelta remaining_time() const;
  int64 remaining_minutes() const;
  // Returns plan data remaining in bytes.
  int64 remaining_data() const;
  // TODO(stevenjb): Make these private with accessors and properly named.
  std::string plan_name;
  CellularDataPlanType plan_type;
  base::Time update_time;
  base::Time plan_start_time;
  base::Time plan_end_time;
  int64 plan_data_bytes;
  int64 data_bytes_used;
};

typedef ScopedVector<CellularDataPlan> CellularDataPlanVector;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CELLULAR_DATA_PLAN_H_
