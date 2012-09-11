// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_UI_CONSTANTS_H_
#define CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_UI_CONSTANTS_H_

#include "base/basictypes.h"

namespace performance_monitor {

enum EventCategory {
  EVENT_CATEGORY_EXTENSIONS,
  EVENT_CATEGORY_CHROME,
  EVENT_CATEGORY_EXCEPTIONS,
  EVENT_CATEGORY_NUMBER_OF_CATEGORIES
};

enum MetricCategory {
  METRIC_CATEGORY_CPU,
  METRIC_CATEGORY_MEMORY,
  METRIC_CATEGORY_TIMING,
  METRIC_CATEGORY_NETWORK,
  METRIC_CATEGORY_NUMBER_OF_CATEGORIES
};

enum Unit {
  UNIT_BYTES,
  UNIT_KILOBYTES,
  UNIT_MEGABYTES,
  UNIT_GIGABYTES,
  UNIT_TERABYTES,
  UNIT_MICROSECONDS,
  UNIT_MILLISECONDS,
  UNIT_SECONDS,
  UNIT_MINUTES,
  UNIT_HOURS,
  UNIT_DAYS,
  UNIT_WEEKS,
  UNIT_MONTHS,
  UNIT_YEARS,
  UNIT_PERCENT,
  UNIT_UNDEFINED
};

// A MeasurementType represents the "type" of data which we are measuring, e.g.
// whether we are measuring distance, memory amounts, time, etc. Two units can
// be converted if and only if they are in the same type. We can convert
// two units of distance (meters to centimeters), but cannot convert a unit of
// time to a unit of memory (seconds to megabytes).
enum MeasurementType {
  MEASUREMENT_TYPE_MEMORY,
  MEASUREMENT_TYPE_PERCENT,
  MEASUREMENT_TYPE_TIME,
  MEASUREMENT_TYPE_UNDEFINED
};

// A struct which holds the conversion information for each unit. The
// |amount_in_base_units| corresponds to the value of 1 |unit| in the specified
// base for the measurement type (for instance, since the base unit for memory
// is bytes, a kilobyte would have |amount_in_base_units| of 1 << 10.
struct UnitDetails {
  const Unit unit;
  const MeasurementType measurement_type;
  const int64 amount_in_base_units;
};

// Returns the corresponding UnitDetails for the given unit, or NULL if invalid.
const UnitDetails* GetUnitDetails(Unit unit);

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_UI_CONSTANTS_H_
