// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/calculator.h"

#include <algorithm>
#include <set>

#include "base/metrics/histogram_macros.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace responsiveness {

namespace {

// We divide the measurement interval into discretized time slices.
// Each slice is marked as janky if it contained a janky task. A janky task is
// one whose execution latency is greater than kJankThreshold.
constexpr base::TimeDelta kMeasurementInterval =
    base::TimeDelta::FromSeconds(30);

// A task or event longer than kJankThreshold is considered janky.
constexpr base::TimeDelta kJankThreshold =
    base::TimeDelta::FromMilliseconds(100);

// If there have been no events/tasks on the UI thread for a significant period
// of time, it's likely because Chrome was suspended.
// This value is copied from queueing_time_estimator.cc:kInvalidPeriodThreshold.
constexpr base::TimeDelta kSuspendInterval = base::TimeDelta::FromSeconds(30);

// Given a |jank|, finds each janky slice between |start_time| and |end_time|,
// and adds it to |janky_slices|.
void AddJankySlices(std::set<int>* janky_slices,
                    const Calculator::Jank& jank,
                    base::TimeTicks start_time,
                    base::TimeTicks end_time) {
  // Ignore the first jank threshold, since that's the part of the task/event
  // that wasn't janky.
  base::TimeTicks jank_start = jank.start_time + kJankThreshold;

  // Bound by |start_time| and |end_time|.
  jank_start = std::max(jank_start, start_time);
  base::TimeTicks jank_end = std::min(jank.end_time, end_time);

  // Find each janky slice, and add it to |janky_slices|.
  while (jank_start < jank_end) {
    // Convert |jank_start| to a slice label.
    int64_t label = (jank_start - start_time) / kJankThreshold;
    janky_slices->insert(label);

    jank_start += kJankThreshold;
  }
}

}  // namespace

Calculator::Jank::Jank(base::TimeTicks start_time, base::TimeTicks end_time)
    : start_time(start_time), end_time(end_time) {
  DCHECK_LE(start_time, end_time);
}

Calculator::Calculator()
    : last_calculation_time_(base::TimeTicks::Now()),
      most_recent_activity_time_(last_calculation_time_) {}
Calculator::~Calculator() = default;

void Calculator::TaskOrEventFinishedOnUIThread(
    base::TimeTicks queue_time,
    base::TimeTicks execution_start_time,
    base::TimeTicks execution_finish_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_GE(execution_start_time, queue_time);

  if (execution_finish_time - queue_time >= kJankThreshold) {
    GetQueueAndExecutionJanksOnUIThread().emplace_back(queue_time,
                                                       execution_finish_time);
    if (execution_finish_time - execution_start_time >= kJankThreshold) {
      GetExecutionJanksOnUIThread().emplace_back(execution_start_time,
                                                 execution_finish_time);
    }
  }

  // We rely on the assumption that |finish_time| is the current time.
  CalculateResponsivenessIfNecessary(/*current_time=*/execution_finish_time);
}

void Calculator::TaskOrEventFinishedOnIOThread(
    base::TimeTicks queue_time,
    base::TimeTicks execution_start_time,
    base::TimeTicks execution_finish_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK_GE(execution_start_time, queue_time);

  if (execution_finish_time - queue_time >= kJankThreshold) {
    base::AutoLock lock(io_thread_lock_);
    queue_and_execution_janks_on_io_thread_.emplace_back(queue_time,
                                                         execution_finish_time);
    if (execution_finish_time - execution_start_time >= kJankThreshold) {
      execution_janks_on_io_thread_.emplace_back(execution_start_time,
                                                 execution_finish_time);
    }
  }
}

void Calculator::EmitResponsiveness(JankType jank_type, size_t janky_slices) {
  constexpr size_t kMaxJankySlices = 300;
  DCHECK_LE(janky_slices, kMaxJankySlices);
  switch (jank_type) {
    case JankType::kExecution: {
      UMA_HISTOGRAM_COUNTS_1000(
          "Browser.Responsiveness.JankyIntervalsPerThirtySeconds",
          janky_slices);
      break;
    }
    case JankType::kQueueAndExecution: {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Browser.Responsiveness.JankyIntervalsPerThirtySeconds2",
          janky_slices, 1, kMaxJankySlices, 50);
      break;
    }
  }
}

base::TimeTicks Calculator::GetLastCalculationTime() {
  return last_calculation_time_;
}

void Calculator::CalculateResponsivenessIfNecessary(
    base::TimeTicks current_time) {
  base::TimeTicks last_activity_time = most_recent_activity_time_;
  most_recent_activity_time_ = current_time;

  // We intentionally dump all data if it appears that Chrome was suspended.
  // [e.g. machine is asleep, process is backgrounded on Android]. We don't have
  // an explicit signal for this. Instead, we rely on the assumption that when
  // Chrome is not suspended, there is a steady stream of tasks and events on
  // the UI thread. If there's been a significant amount of time since the last
  // calculation, then it's likely because Chrome was suspended.
  if (current_time - last_activity_time > kSuspendInterval) {
    last_calculation_time_ = current_time;
    GetExecutionJanksOnUIThread().clear();
    GetQueueAndExecutionJanksOnUIThread().clear();
    {
      base::AutoLock lock(io_thread_lock_);
      execution_janks_on_io_thread_.clear();
      queue_and_execution_janks_on_io_thread_.clear();
    }
    return;
  }

  base::TimeDelta time_since_last_calculation =
      current_time - last_calculation_time_;
  if (time_since_last_calculation <= kMeasurementInterval)
    return;

  // At least |kMeasurementInterval| time has passed, so we want to move forward
  // |last_calculation_time_| and make measurements based on janks in that
  // interval.
  int64_t number_of_measurement_intervals =
      time_since_last_calculation / kMeasurementInterval;
  DCHECK_GE(number_of_measurement_intervals, 1);

  const base::TimeTicks new_calculation_time =
      last_calculation_time_ +
      number_of_measurement_intervals * kMeasurementInterval;

  // Acquire the janks in the measurement interval from the UI and IO threads.
  std::vector<JankList> execution_janks_from_multiple_threads;
  std::vector<JankList> queue_and_execution_janks_from_multiple_threads;
  execution_janks_from_multiple_threads.push_back(TakeJanksOlderThanTime(
      &GetExecutionJanksOnUIThread(), new_calculation_time));
  queue_and_execution_janks_from_multiple_threads.push_back(
      TakeJanksOlderThanTime(&GetQueueAndExecutionJanksOnUIThread(),
                             new_calculation_time));
  {
    base::AutoLock lock(io_thread_lock_);
    execution_janks_from_multiple_threads.push_back(TakeJanksOlderThanTime(
        &execution_janks_on_io_thread_, new_calculation_time));
    queue_and_execution_janks_from_multiple_threads.push_back(
        TakeJanksOlderThanTime(&queue_and_execution_janks_on_io_thread_,
                               new_calculation_time));
  }

  CalculateResponsiveness(JankType::kExecution,
                          std::move(execution_janks_from_multiple_threads),
                          last_calculation_time_, new_calculation_time);
  CalculateResponsiveness(
      JankType::kQueueAndExecution,
      std::move(queue_and_execution_janks_from_multiple_threads),
      last_calculation_time_, new_calculation_time);

  last_calculation_time_ = new_calculation_time;
}

void Calculator::CalculateResponsiveness(
    JankType jank_type,
    std::vector<JankList> janks_from_multiple_threads,
    base::TimeTicks start_time,
    base::TimeTicks end_time) {
  while (start_time < end_time) {
    const base::TimeTicks current_interval_end_time =
        start_time + kMeasurementInterval;

    // We divide the current measurement interval into slices. Each slice is
    // given a monotonically increasing label, from 0 to |kNumberOfSlices - 1|.
    // Example [all times in milliseconds since UNIX epoch]:
    //   The measurement interval is [50135, 80135].
    //   The slice [50135, 50235] is labeled 0.
    //   The slice [50235, 50335] is labeled 1.
    //   ...
    //   The slice [80035, 80135] is labeled 299.
    std::set<int> janky_slices;

    for (const JankList& janks : janks_from_multiple_threads) {
      for (const Jank& jank : janks) {
        AddJankySlices(&janky_slices, jank, start_time,
                       current_interval_end_time);
      }
    }

    EmitResponsiveness(jank_type, janky_slices.size());

    start_time = current_interval_end_time;
  }
}

Calculator::JankList& Calculator::GetExecutionJanksOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return execution_janks_on_ui_thread_;
}

Calculator::JankList& Calculator::GetQueueAndExecutionJanksOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return queue_and_execution_janks_on_ui_thread_;
}

Calculator::JankList Calculator::TakeJanksOlderThanTime(
    JankList* janks,
    base::TimeTicks end_time) {
  // Find all janks with Jank.start_time < |end_time|.
  auto it = std::partition(
      janks->begin(), janks->end(),
      [&end_time](const Jank& jank) { return jank.start_time < end_time; });

  // Early exit. We don't need to remove any Janks either, since Jank.end_time
  // >= Jank.start_time.
  if (it == janks->begin())
    return JankList();

  JankList janks_to_return(janks->begin(), it);

  // Remove all janks with Jank.end_time < |end_time|.
  auto first_jank_to_keep = std::partition(
      janks->begin(), janks->end(),
      [&end_time](const Jank& jank) { return jank.end_time < end_time; });
  janks->erase(janks->begin(), first_jank_to_keep);
  return janks_to_return;
}

}  // namespace responsiveness
}  // namespace content
