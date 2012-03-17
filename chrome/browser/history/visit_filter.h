// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_VISIT_FILTER_H_
#define CHROME_BROWSER_HISTORY_VISIT_FILTER_H_
#pragma once

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time.h"

namespace history {

// Helper class for creation of filters for VisitDatabase that is used to filter
// out visits by time of the day, day of the week, workdays, holidays, duration
// of the visit, location and the combinations of that.
// It also stores sorting order of the returned resilts.
class VisitFilter {
 public:
  VisitFilter();
  virtual ~VisitFilter();

  // Vector of time intervals [begin time, end time]. All of the following
  // functions produce vectors that are sorted in order from most recent to
  // least recent and have intervals that do not intersect.
  // |first| always points to the beginning of the time period, |second| - to
  // the end.
  typedef std::vector<std::pair<base::Time, base::Time> > TimeVector;

  // Returns time vector associated with the object.
  const TimeVector& times() const {
    return times_;
  }

  // Sets |max_results| of the results to be returned. 0 means "return results
  // for the two months prior to passed time".
  void set_max_results(size_t max_results) {
    max_results_ = max_results;
    if (times_.size() > max_results_)
      times_.resize(max_results_);
  }

  // Sets time in range during the day, for example, the following code would
  // produce time vector with following values: 1/19/2005 9:07:06PM-9:27:06PM,
  // 1/18/2005 9:07:06PM-9:27:06PM, 1/17/2005 9:07:06PM-9:27:06PM, etc.
  // base::Time::Exploded et = { 2005, 1, 0, 19, 21, 17, 6, 0 };
  // base::Time t(base::Time::FromLocalExploded(et));
  // base::TimeDelta ten_minutes(base::TimeDelta::FromMinutes(10));
  // VisitFilter f;
  // f.SetTimeInRangeFilter(t - ten_minutes, t + ten_minutes);
  // This filter could be applied simultaneously with day filters.
  void SetTimeInRangeFilter(base::Time begin_time_of_the_day,
                            base::Time end_time_of_the_day);

  // The following two filters are exclusive - setting one, clears the other
  // one. But both of them could be used with SetTimeInRangeFilter().

  // Sets time in range for the day of the week staring with the day on the
  // |week|. The intervals counted from midnight to midnight.
  // |day| - day of the week: 0 - sunday, 1 - monday, etc.
  // |week| - the week relative to which everything is calculated. If |week| is
  // unset, the current week is used.
  void SetDayOfTheWeekFilter(int day, base::Time week);

  // Sets time in range for the holidays or workdays of the week staring with
  // |week|. The intervals counted from midnight of the first workday/holiday
  // to midnight of the last workday/holiday.
  // |workday| - if true means Monday-Friday, if false means Saturday-Sunday.
  // TODO(georgey) - internationalize it.
  // |week| - the week relative to which everything is calculated. If |week| is
  // unset, the current week is used.
  void SetDayTypeFilter(bool workday, base::Time week);

  // Sorting order that results after applying this filter are sorted by.
  enum SortingOrder {
    ORDER_BY_RECENCY,  // Most recent visits are most relevant ones. (default)
    ORDER_BY_VISIT_COUNT,  // Most visited are listed first.
    ORDER_BY_DURATION_SPENT,  // The sites that user spents more time in are
                              // sorted first.
  };

  void set_sorting_order(SortingOrder order) {
    sorting_order_ = order;
  }

  SortingOrder sorting_order() const {
    return sorting_order_;
  }

  // Clears all of the filters.
  void ClearFilters();

 private:
  FRIEND_TEST_ALL_PREFIXES(VisitFilterTest, CheckFilters);
  FRIEND_TEST_ALL_PREFIXES(VisitFilterTest, GetTimesInRange);
  FRIEND_TEST_ALL_PREFIXES(VisitFilterTest, GetTimesOnTheDayOfTheWeek);
  FRIEND_TEST_ALL_PREFIXES(VisitFilterTest, GetTimesOnTheSameDayType);
  FRIEND_TEST_ALL_PREFIXES(VisitFilterTest, UniteTimeVectors);
  FRIEND_TEST_ALL_PREFIXES(VisitFilterTest, IntersectTimeVectors);

  // Internal helper for the update.
  bool UpdateTimeVector();

  // Internal helper for getting the times in range. See SetTimeInRangeFilter().
  static void GetTimesInRange(base::Time begin_time_of_the_day,
                              base::Time end_time_of_the_day,
                              size_t max_results,
                              TimeVector* times);

  // Internal helper for getting the days in range. See SetDayOfTheWeekFilter().
  // |day| could be outside of the range: -4 (3 - 7) means Wednesday last week,
  // 17 (3 + 2 * 7) means Wednesday in two weeks.
  static void GetTimesOnTheDayOfTheWeek(int day,
                                        base::Time week,
                                        size_t max_results,
                                        TimeVector* times);

  // Internal helper for getting the days in range. See SetDayTypeFilter().
  static void GetTimesOnTheSameDayType(bool workday,
                                       base::Time week,
                                       size_t max_results,
                                       TimeVector* times);

  // Unites two vectors, so the new vector has non-intersecting union of the
  // original ranges. Returns true if the result is non-empty, false otherwise.
  static bool UniteTimeVectors(const TimeVector& vector1,
                               const TimeVector& vector2,
                               TimeVector* result);

  // Intersects two vectors, so the new vector has ranges that are covered by
  // both of the original ranges. Returns true if the result is non-empty, false
  // otherwise.
  static bool IntersectTimeVectors(const TimeVector& vector1,
                                   const TimeVector& vector2,
                                   TimeVector* result);

  base::Time begin_time_of_the_day_;
  base::Time end_time_of_the_day_;
  enum {
    DAY_UNDEFINED = -1,
    WORKDAY = 7,
    HOLIDAY = 8,
  };
  int day_;
  base::Time week_;
  TimeVector times_;
  size_t max_results_;
  SortingOrder sorting_order_;
};

}  // history

#endif  // CHROME_BROWSER_HISTORY_VISIT_FILTER_H_
