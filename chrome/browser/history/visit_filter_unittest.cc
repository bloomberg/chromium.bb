// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/visit_filter.h"

#include <math.h>

#include "base/logging.h"
#include "base/time.h"
#include "chrome/browser/history/history_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// So the tests won't go into the other day +/- several hours, return midday of
// today.
base::Time GetClosestMidday() {
  return base::Time::Now().LocalMidnight() + base::TimeDelta::FromHours(12);
}

}  // namespace

namespace history {

class VisitFilterTest : public testing::Test {
 public:
  VisitFilterTest();

 protected:
  void SetUp();
  void TearDown();
};

VisitFilterTest::VisitFilterTest() {
}

void VisitFilterTest::SetUp() {
}

void VisitFilterTest::TearDown() {
}

TEST_F(VisitFilterTest, CheckFilters) {
  base::Time t(GetClosestMidday());
  base::TimeDelta two_hours(base::TimeDelta::FromHours(2));
  VisitFilter f;
  f.set_max_results(21U);
  f.SetFilterTime(t);
  f.SetFilterWidth(two_hours);
  EXPECT_EQ(21U, f.times().size());
  for (size_t i = 0; i < f.times().size(); ++i) {
    base::Time t_interval(t);
    t_interval -= base::TimeDelta::FromDays(i);
    EXPECT_EQ(t_interval - two_hours, f.times()[i].first) <<
        "Fails at index:" << i;
    EXPECT_EQ(t_interval + two_hours, f.times()[i].second) <<
        "Fails at index:" << i;
  }
  base::Time::Exploded et;
  t.LocalExplode(&et);
  f.SetDayOfTheWeekFilter(et.day_of_week);
  // 3 weeks in 21 days.
  ASSERT_EQ(3U, f.times().size());
  for (size_t i = 1; i < f.times().size(); ++i) {
    base::Time t_interval(t);
    t_interval -= base::TimeDelta::FromDays(i);
    EXPECT_EQ(f.times()[i].first + base::TimeDelta::FromDays(7),
              f.times()[i - 1].first) <<
        "Fails at index:" << i;
    EXPECT_EQ(f.times()[i].second + base::TimeDelta::FromDays(7),
              f.times()[i - 1].second) <<
        "Fails at index:" << i;
    EXPECT_EQ(two_hours * 2,
              f.times()[i].second - f.times()[i].first) <<
        "Fails at index:" << i;
  }
}

TEST_F(VisitFilterTest, GetTimesInRange) {
  base::Time::Exploded et = { 2011, 7, 0, 19, 22, 15, 11, 0 };
  base::Time t(base::Time::FromLocalExploded(et));
  base::TimeDelta two_hours(base::TimeDelta::FromHours(2));
  VisitFilter::TimeVector times;
  VisitFilter::GetTimesInRange(t - two_hours, t + two_hours, 10U, &times);
  EXPECT_GT(11U, times.size());
  for (size_t i = 0; i < times.size(); ++i) {
    base::Time t_interval(t);
    t_interval -= base::TimeDelta::FromDays(i);
    EXPECT_EQ(t_interval - two_hours, times[i].first) << "Fails at index:" << i;
    EXPECT_EQ(t_interval + two_hours, times[i].second) <<
        "Fails at index:" << i;
  }
}

TEST_F(VisitFilterTest, GetTimesOnTheDayOfTheWeek) {
  base::Time t(GetClosestMidday());
  VisitFilter::TimeVector times;
  base::Time::Exploded et;
  t.LocalExplode(&et);
  VisitFilter::GetTimesOnTheDayOfTheWeek(et.day_of_week, t, 10U, &times);
  EXPECT_GT(11U, times.size());
  et.hour = 0;
  et.minute = 0;
  et.second = 0;
  et.millisecond = 0;
  for (size_t i = 0; i < times.size(); ++i) {
    base::Time t_interval(base::Time::FromLocalExploded(et));
    t_interval -= base::TimeDelta::FromDays(7 * i);
    EXPECT_EQ(t_interval, times[i].first) << "Fails at index:" << i;
    EXPECT_EQ(t_interval + base::TimeDelta::FromDays(1), times[i].second) <<
        "Fails at index:" << i;
  }
}

TEST_F(VisitFilterTest, GetTimesOnTheSameDayType) {
  base::Time::Exploded et = { 2011, 7, 0, 19, 22, 15, 11, 0 };
  base::Time t(base::Time::FromLocalExploded(et));
  VisitFilter::TimeVector times;
  t.LocalExplode(&et);
  VisitFilter::GetTimesOnTheSameDayType(et.day_of_week, t, 10U, &times);
  EXPECT_GT(11U, times.size());
  et.hour = 0;
  et.minute = 0;
  et.second = 0;
  et.millisecond = 0;
  base::Time t_start(base::Time::FromLocalExploded(et));
  base::TimeDelta t_length;
  if (et.day_of_week == 0 || et.day_of_week == 6) {
    // Sunday and Saturday.
    t_length = base::TimeDelta::FromDays(2);
    if (et.day_of_week == 0)
      t_start -= base::TimeDelta::FromDays(1);
  } else {
    t_length = base::TimeDelta::FromDays(5);
    if (et.day_of_week != 1)
      t_start -= base::TimeDelta::FromDays(et.day_of_week - 1);
  }
  for (size_t i = 0; i < times.size(); ++i) {
    base::Time t_interval(t_start);
    t_interval -= base::TimeDelta::FromDays(7 * i);
    EXPECT_EQ(t_interval, times[i].first) << "Fails at index:" << i;
    EXPECT_EQ(t_interval + t_length, times[i].second) << "Fails at index:" << i;
  }
}

TEST_F(VisitFilterTest, UniteTimeVectors) {
  base::Time t(base::Time::Now());
  base::TimeDelta one_hour(base::TimeDelta::FromHours(1));
  base::TimeDelta one_day(base::TimeDelta::FromDays(1));
  VisitFilter::TimeVector times1;
  times1.push_back(std::make_pair(t - one_hour, t + one_hour));
  times1.push_back(std::make_pair(t - one_hour - one_day,
                                  t + one_hour - one_day));
  times1.push_back(std::make_pair(t - one_hour - one_day * 2,
                                  t + one_hour - one_day * 2));
  times1.push_back(std::make_pair(t - one_hour - one_day * 3,
                                  t + one_hour - one_day * 3));

  VisitFilter::TimeVector times2;
  // Should lie completely within times1[0].
  times2.push_back(std::make_pair(t - one_hour / 2, t + one_hour / 2));
  // Should lie just before times1[1].
  times2.push_back(std::make_pair(t + one_hour * 2 - one_day,
                                  t + one_hour * 3 - one_day));
  // Should intersect with times1.
  times2.push_back(std::make_pair(t - one_day * 2,
                                  t + one_hour * 2 - one_day * 2));
  times2.push_back(std::make_pair(t - one_hour * 2 - one_day * 3,
                                  t - one_day * 3));

  VisitFilter::TimeVector result;
  EXPECT_TRUE(VisitFilter::UniteTimeVectors(times1, times2, &result));
  ASSERT_EQ(5U, result.size());
  EXPECT_EQ(t - one_hour, result[0].first);
  EXPECT_EQ(t + one_hour, result[0].second);
  EXPECT_EQ(t + one_hour * 2 - one_day, result[1].first);
  EXPECT_EQ(t + one_hour * 3 - one_day, result[1].second);
  EXPECT_EQ(t - one_hour - one_day, result[2].first);
  EXPECT_EQ(t + one_hour - one_day, result[2].second);
  EXPECT_EQ(t - one_hour - one_day * 2, result[3].first);
  EXPECT_EQ(t + one_hour * 2 - one_day * 2, result[3].second);
  EXPECT_EQ(t - one_hour * 2 - one_day * 3, result[4].first);
  EXPECT_EQ(t + one_hour - one_day * 3, result[4].second);

  EXPECT_FALSE(VisitFilter::UniteTimeVectors(VisitFilter::TimeVector(),
                                             VisitFilter::TimeVector(),
                                             &result));
  EXPECT_TRUE(result.empty());
}

TEST_F(VisitFilterTest, IntersectTimeVectors) {
  base::Time t(base::Time::Now());
  base::TimeDelta one_hour(base::TimeDelta::FromHours(1));
  base::TimeDelta one_day(base::TimeDelta::FromDays(1));
  VisitFilter::TimeVector times1;
  times1.push_back(std::make_pair(t - one_hour, t + one_hour));

  VisitFilter::TimeVector times2;
  // Should lie just before times1[0].
  times2.push_back(std::make_pair(t + one_hour * 2,
                                  t + one_hour * 3));

  VisitFilter::TimeVector result;
  EXPECT_FALSE(VisitFilter::IntersectTimeVectors(times1, times2, &result));
  EXPECT_TRUE(result.empty());

  times1.push_back(std::make_pair(t - one_hour - one_day,
                                  t + one_hour - one_day));
  times1.push_back(std::make_pair(t - one_hour - one_day * 2,
                                  t + one_hour - one_day * 2));
  times1.push_back(std::make_pair(t - one_hour - one_day * 3,
                                  t + one_hour - one_day * 3));

  // Should lie completely within times1[1].
  times2.push_back(std::make_pair(t - one_hour / 2 - one_day,
                                  t + one_hour / 2 - one_day));
  // Should intersect with times1.
  times2.push_back(std::make_pair(t - one_day * 2,
                                  t + one_hour * 2 - one_day * 2));
  times2.push_back(std::make_pair(t - one_hour * 2 - one_day * 3,
                                  t - one_day * 3));

  EXPECT_TRUE(VisitFilter::IntersectTimeVectors(times1, times2, &result));
  ASSERT_EQ(3U, result.size());
  EXPECT_EQ(t - one_hour / 2 - one_day, result[0].first);
  EXPECT_EQ(t + one_hour / 2 - one_day, result[0].second);
  EXPECT_EQ(t - one_day * 2, result[1].first);
  EXPECT_EQ(t + one_hour - one_day * 2, result[1].second);
  EXPECT_EQ(t - one_hour - one_day * 3, result[2].first);
  EXPECT_EQ(t - one_day * 3, result[2].second);

  // Check that touching ranges do not intersect.
  times1.clear();
  times1.push_back(std::make_pair(t - one_hour, t));
  times2.clear();
  times2.push_back(std::make_pair(t, t + one_hour));
  EXPECT_FALSE(VisitFilter::IntersectTimeVectors(times1, times2, &result));
  EXPECT_TRUE(result.empty());
}

TEST_F(VisitFilterTest, GetVisitScore) {
  base::Time filter_time = GetClosestMidday();
  VisitFilter filter;
  VisitRow visit;

  filter.set_sorting_order(VisitFilter::ORDER_BY_RECENCY);
  filter.SetFilterTime(filter_time);
  filter.SetFilterWidth(base::TimeDelta::FromHours(1));

  double one_week_one_hour_staleness = pow(2, -(24.0 * 7.0 + 1.0) /
                                               (24.0 * 7.0));

  // No decay on current visit.
  visit.visit_time = filter_time;
  EXPECT_DOUBLE_EQ(1.0, filter.GetVisitScore(visit));
  // Half score after a week.
  visit.visit_time = filter_time - base::TimeDelta::FromDays(7);
  EXPECT_DOUBLE_EQ(0.5, filter.GetVisitScore(visit));
  // Future visits should be treated as current.
  visit.visit_time = filter_time + base::TimeDelta::FromDays(1);
  EXPECT_DOUBLE_EQ(1.0, filter.GetVisitScore(visit));

  filter.set_sorting_order(VisitFilter::ORDER_BY_VISIT_COUNT);
  // Every visit should score 1 with this filter.
  visit.visit_time = filter_time;
  EXPECT_DOUBLE_EQ(1.0, filter.GetVisitScore(visit));
  visit.visit_time = filter_time - base::TimeDelta::FromDays(7);
  EXPECT_DOUBLE_EQ(1.0, filter.GetVisitScore(visit));
  visit.visit_time = filter_time + base::TimeDelta::FromDays(7);
  EXPECT_DOUBLE_EQ(1.0, filter.GetVisitScore(visit));

  filter.set_sorting_order(VisitFilter::ORDER_BY_TIME_LINEAR);
  visit.visit_time = filter_time;
  EXPECT_DOUBLE_EQ(1.0, filter.GetVisitScore(visit));
  // Half the filter width forward in time should get half the score for the
  // time difference, but no staleness decay.
  visit.visit_time = filter_time + base::TimeDelta::FromMinutes(30);
  EXPECT_DOUBLE_EQ(0.5, filter.GetVisitScore(visit));
  // One week back in time gets full time difference score, but a staleness
  // factor of 0.5
  visit.visit_time = filter_time - base::TimeDelta::FromDays(7);
  EXPECT_DOUBLE_EQ(0.5, filter.GetVisitScore(visit));
  // One week plus half a filter width should have it's score halved before
  // the staleness factor.
  filter.SetFilterWidth(base::TimeDelta::FromHours(2));
  visit.visit_time = filter_time - base::TimeDelta::FromDays(7) -
                     base::TimeDelta::FromHours(1);
  EXPECT_DOUBLE_EQ(0.5 * one_week_one_hour_staleness,
                   filter.GetVisitScore(visit));
  filter.SetFilterWidth(base::TimeDelta::FromHours(1));

  filter.set_sorting_order(VisitFilter::ORDER_BY_TIME_GAUSSIAN);
  visit.visit_time = filter_time;
  EXPECT_DOUBLE_EQ(1.0, filter.GetVisitScore(visit));
  // Going forward in time to test the normal distribution function.
  visit.visit_time = filter_time + base::TimeDelta::FromHours(1);
  EXPECT_DOUBLE_EQ(exp(-0.5), filter.GetVisitScore(visit));
  visit.visit_time = filter_time + base::TimeDelta::FromMinutes(30);
  EXPECT_DOUBLE_EQ(exp(-0.125), filter.GetVisitScore(visit));
  // One week back in time gets full time difference score, but a staleness
  // factor of 0.5
  visit.visit_time = filter_time - base::TimeDelta::FromDays(7);
  EXPECT_DOUBLE_EQ(0.5, filter.GetVisitScore(visit));
  // One standard deviation of decay, plus the staleness factor.
  visit.visit_time = filter_time - base::TimeDelta::FromDays(7) -
                     base::TimeDelta::FromHours(1);
  EXPECT_DOUBLE_EQ(exp(-0.5) * one_week_one_hour_staleness,
                   filter.GetVisitScore(visit));
}

}  // namespace history
