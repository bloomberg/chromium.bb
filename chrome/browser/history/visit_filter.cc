// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/visit_filter.h"

#include <math.h>

#include <algorithm>

#include "base/logging.h"
#include "base/time.h"
#include "chrome/browser/history/history_types.h"

namespace history {

const double kLn2 = 0.6931471805599453;

VisitFilter::VisitFilter()
    : day_(DAY_UNDEFINED),
      max_results_(0),
      sorting_order_(ORDER_BY_RECENCY) {
}

VisitFilter::~VisitFilter() {
}

void VisitFilter::SetFilterTime(const base::Time& filter_time) {
  filter_time_ = filter_time;
  UpdateTimeVector();
}

void VisitFilter::SetFilterWidth(const base::TimeDelta& filter_width) {
  filter_width_ = filter_width;
  UpdateTimeVector();
}

void VisitFilter::SetDayOfTheWeekFilter(int day) {
  day_ = day;
  UpdateTimeVector();
}

void VisitFilter::SetDayTypeFilter(bool workday) {
  day_ = workday ? WORKDAY : HOLIDAY;
  UpdateTimeVector();
}

void VisitFilter::ClearFilters() {
  filter_time_ = base::Time();
  filter_width_ = base::TimeDelta::FromHours(0);
  day_ = DAY_UNDEFINED;
  UpdateTimeVector();
}

bool VisitFilter::UpdateTimeVector() {

  TimeVector days_of_the_week;
  if (day_ >= 0 && day_ <= 6) {
    GetTimesOnTheDayOfTheWeek(day_, filter_time_, max_results_,
                              &days_of_the_week);
  } else if (day_ == WORKDAY || day_ == HOLIDAY) {
    GetTimesOnTheSameDayType(
        (day_ == WORKDAY), filter_time_, max_results_, &days_of_the_week);
  }

  TimeVector times_of_the_day;
  if (filter_width_ != base::TimeDelta::FromSeconds(0)) {
    if (sorting_order_ == ORDER_BY_TIME_GAUSSIAN) {
      // Limit queries to 5 standard deviations.
      GetTimesInRange(filter_time_ - 5 * filter_width_,
                      filter_time_ + 5 * filter_width_,
                      max_results_, &times_of_the_day);
    } else {
      GetTimesInRange(filter_time_ - filter_width_,
                      filter_time_ + filter_width_,
                      max_results_, &times_of_the_day);
    }
  }

  if (times_of_the_day.empty()) {
    if (days_of_the_week.empty())
      times_.clear();
    else
      times_.swap(days_of_the_week);
  } else {
    if (days_of_the_week.empty())
      times_.swap(times_of_the_day);
    else
      IntersectTimeVectors(times_of_the_day, days_of_the_week, &times_);
  }

  return !times_.empty();
}

// static
void VisitFilter::GetTimesInRange(base::Time begin_time_of_the_day,
                                  base::Time end_time_of_the_day,
                                  size_t max_results,
                                  TimeVector* times) {
  DCHECK(times);
  times->clear();
  times->reserve(max_results);
  const size_t kMaxReturnedResults = 62;  // 2 months (<= 62 days).

  if (!max_results)
    max_results = kMaxReturnedResults;

  // If range is more than 24 hours, return a contiguous interval covering
  // |max_results| days.
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);
  if (end_time_of_the_day - begin_time_of_the_day >= one_day) {
    times->push_back(
        std::make_pair(begin_time_of_the_day - one_day * (max_results - 1),
                       end_time_of_the_day));
    return;
  }

  for (size_t i = 0; i < max_results; ++i) {
    times->push_back(
        std::make_pair(begin_time_of_the_day - base::TimeDelta::FromDays(i),
                       end_time_of_the_day - base::TimeDelta::FromDays(i)));
  }
}

double VisitFilter::GetVisitScore(const VisitRow& visit) const {
  // Decay score by half each week.
  base::TimeDelta time_passed = filter_time_ - visit.visit_time;
  // Clamp to 0 in case time jumps backwards (e.g. due to DST).
  double decay_exponent = std::max(0.0, kLn2 * static_cast<double>(
      time_passed.InMicroseconds()) / base::Time::kMicrosecondsPerWeek);
  double staleness = 1.0 / exp(decay_exponent);

  double score = 0;
  switch (sorting_order()) {
    case ORDER_BY_RECENCY:
      score = 1.0;  // Let the staleness factor take care of it.
      break;
    case ORDER_BY_VISIT_COUNT:
      score = 1.0;  // Every visit counts the same.
      staleness = 1.0;  // No decay on this one.
      break;
    case ORDER_BY_TIME_GAUSSIAN: {
      double offset =
          GetTimeOfDayDifference(filter_time_,
                                 visit.visit_time).InMicroseconds();
      double sd = filter_width_.InMicroseconds();

      // Calculate score using the normal distribution density function.
      score = exp(-(offset * offset) / (2 * sd * sd));
      break;
    }
    case ORDER_BY_TIME_LINEAR: {
      base::TimeDelta offset = GetTimeOfDayDifference(filter_time_,
                                                      visit.visit_time);
      if (offset > filter_width_) {
        score = 0;
      } else {
        score = 1 - offset.InMicroseconds() / static_cast<double>(
            filter_width_.InMicroseconds());
      }
      break;
    }
    case ORDER_BY_DURATION_SPENT:
    default:
      NOTREACHED() << "Not implemented!";
  }
  return staleness * score;
}

base::TimeDelta
VisitFilter::GetTimeOfDayDifference(base::Time t1, base::Time t2) {
  base::TimeDelta time_of_day1 = t1 - t1.LocalMidnight();
  base::TimeDelta time_of_day2 = t2 - t2.LocalMidnight();

  base::TimeDelta difference;
  if (time_of_day1 < time_of_day2)
    difference = time_of_day2 - time_of_day1;
  else
    difference = time_of_day1 - time_of_day2;

  // If the difference is more than 12 hours, we'll get closer by 'wrapping'
  // around the day barrier.
  if (difference > base::TimeDelta::FromHours(12))
    difference = base::TimeDelta::FromHours(24) - difference;

  return difference;
}

// static
void VisitFilter::GetTimesOnTheDayOfTheWeek(int day,
                                            base::Time week,
                                            size_t max_results,
                                            TimeVector* times) {
  DCHECK(times);

  base::Time::Exploded exploded_time;
  if (week.is_null())
    week = base::Time::Now();
  week.LocalExplode(&exploded_time);
  base::TimeDelta shift = base::TimeDelta::FromDays(
      exploded_time.day_of_week - day);

  base::Time day_base = week.LocalMidnight();
  day_base -= shift;

  times->clear();
  times->reserve(max_results);

  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  const size_t kMaxReturnedResults = 9;  // 2 months (<= 9 weeks).

  if (!max_results)
    max_results = kMaxReturnedResults;

  for (size_t i = 0; i < max_results; ++i) {
    times->push_back(
        std::make_pair(day_base - base::TimeDelta::FromDays(i * 7),
                       day_base + one_day - base::TimeDelta::FromDays(i * 7)));
  }
}

// static
void VisitFilter::GetTimesOnTheSameDayType(bool workday,
                                           base::Time week,
                                           size_t max_results,
                                           TimeVector* times) {
  DCHECK(times);
  if (week.is_null())
    week = base::Time::Now();
  // TODO(georgey): internationalize workdays/weekends/holidays.
  if (!workday) {
    TimeVector sunday;
    TimeVector saturday;
    base::Time::Exploded exploded_time;
    week.LocalExplode(&exploded_time);

    GetTimesOnTheDayOfTheWeek(exploded_time.day_of_week ? 7 : 0, week,
                              max_results, &sunday);
    GetTimesOnTheDayOfTheWeek(exploded_time.day_of_week ? 6 : -1, week,
                              max_results, &saturday);
    UniteTimeVectors(sunday, saturday, times);
    if (max_results && times->size() > max_results)
      times->resize(max_results);
  } else {
    TimeVector vectors[3];
    GetTimesOnTheDayOfTheWeek(1, week, max_results, &vectors[0]);
    for (size_t i = 2; i <= 5; ++i) {
      GetTimesOnTheDayOfTheWeek(i, week, max_results, &vectors[(i - 1) % 3]);
      UniteTimeVectors(vectors[(i - 2) % 3], vectors[(i - 1) % 3],
                       &vectors[i % 3]);
      if (max_results && vectors[i % 3].size() > max_results)
        vectors[i % 3].resize(max_results);
      vectors[i % 3].swap(vectors[(i - 1) % 3]);
    }
    // 1 == 5 - 1 % 3
    times->swap(vectors[1]);
  }
}

// static
bool VisitFilter::UniteTimeVectors(const TimeVector& vector1,
                                   const TimeVector& vector2,
                                   TimeVector* result) {
  // The vectors are sorted going back in time, but each pair has |first| as the
  // beginning of time period and |second| as the end, for example:
  // { 19:20, 20:00 } { 17:00, 18:10 } { 11:33, 11:35 }...
  // The pairs in one vector are guaranteed not to intersect.
  DCHECK(result);
  result->clear();
  result->reserve(vector1.size() + vector2.size());

  size_t vi[2];
  const TimeVector* vectors[2] = { &vector1, &vector2 };
  for (vi[0] = 0, vi[1] = 0;
       vi[0] < vectors[0]->size() && vi[1] < vectors[1]->size();) {
    std::pair<base::Time, base::Time> united_timeslot;
    // Check which element occurs later (for the following diagrams time is
    // increasing to the right, 'f' means first, 's' means second).
    // after the folowing 2 statements:
    // vectors[iterator_index][vi[iterator_index]]           f---s
    // vectors[1 - iterator_index][vi[1 - iterator_index]]  f---s
    // united_timeslot                                       f---s
    // or
    // vectors[iterator_index][vi[iterator_index]]           f---s
    // vectors[1 - iterator_index][vi[1 - iterator_index]]    f-s
    // united_timeslot                                       f---s
    size_t iterator_index =
        ((*vectors[0])[vi[0]].second >= (*vectors[1])[vi[1]].second) ? 0 : 1;
    united_timeslot = (*vectors[iterator_index])[vi[iterator_index]];
    ++vi[iterator_index];
    bool added_timeslot;
    // Merge all timeslots intersecting with |united_timeslot|.
    do {
      added_timeslot = false;
      for (size_t i = 0; i <= 1; ++i) {
        if (vi[i] < vectors[i]->size() &&
            (*vectors[i])[vi[i]].second >= united_timeslot.first) {
          // vectors[i][vi[i]]           f---s
          // united_timeslot               f---s
          // or
          // united_timeslot            f------s
          added_timeslot = true;
          if ((*vectors[i])[vi[i]].first < united_timeslot.first) {
            // vectors[i][vi[i]]           f---s
            // united_timeslot               f---s
            // results in:
            // united_timeslot             f-----s
            united_timeslot.first = (*vectors[i])[vi[i]].first;
          }
          ++vi[i];
        }
      }
    } while (added_timeslot);
    result->push_back(united_timeslot);
  }
  for (size_t i = 0; i <= 1; ++i) {
    for (; vi[i] < vectors[i]->size(); ++vi[i])
      result->push_back((*vectors[i])[vi[i]]);
  }
  return !result->empty();
}

// static
bool VisitFilter::IntersectTimeVectors(const TimeVector& vector1,
                                       const TimeVector& vector2,
                                       TimeVector* result) {
  DCHECK(result);
  result->clear();
  result->reserve(std::max(vector1.size(), vector2.size()));

  TimeVector::const_iterator vi[2];
  for (vi[0] = vector1.begin(), vi[1] = vector2.begin();
       vi[0] != vector1.end() && vi[1] != vector2.end();) {
    size_t it_index = (vi[0]->second >= vi[1]->second) ? 0 : 1;
    if (vi[it_index]->first >= vi[1 - it_index]->second) {
      // vector 1    ++++
      // vector 2 ++
      ++vi[it_index];
    } else if (vi[it_index]->first >= vi[1 - it_index]->first) {
      // vector 1    ++++
      // vector 2 +++++
      result->push_back(std::make_pair(vi[it_index]->first,
                                       vi[1 - it_index]->second));
      ++vi[it_index];
    } else {
      // vector 1 ++++
      // vector 2  ++
      result->push_back(std::make_pair(vi[1 - it_index]->first,
                                       vi[1 - it_index]->second));
      ++vi[1 - it_index];
    }
  }

  return !result->empty();
}

}  // namespace history
