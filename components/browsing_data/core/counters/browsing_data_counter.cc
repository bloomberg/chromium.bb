// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/browsing_data_counter.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"

namespace browsing_data {

namespace {
static const int kDelayUntilShowCalculatingMs = 140;
static const int kDelayUntilReadyToShowResultMs = 1000;
}

BrowsingDataCounter::BrowsingDataCounter()
    : initialized_(false),
      state_(State::IDLE) {}

BrowsingDataCounter::~BrowsingDataCounter() {}

void BrowsingDataCounter::Init(PrefService* pref_service,
                               const Callback& callback) {
  DCHECK(!initialized_);
  callback_ = callback;
  pref_service_ = pref_service;
  pref_.Init(GetPrefName(), pref_service_,
             base::Bind(&BrowsingDataCounter::Restart, base::Unretained(this)));
  period_.Init(
      browsing_data::prefs::kDeleteTimePeriod, pref_service_,
      base::Bind(&BrowsingDataCounter::Restart, base::Unretained(this)));

  initialized_ = true;
  OnInitialized();
}

void BrowsingDataCounter::OnInitialized() {}

base::Time BrowsingDataCounter::GetPeriodStart() {
  return CalculateBeginDeleteTime(static_cast<TimePeriod>(*period_));
}

void BrowsingDataCounter::Restart() {
  DCHECK(initialized_);
  if (state_ == State::IDLE) {
    DCHECK(!timer_.IsRunning());
    DCHECK(!staged_result_);
  } else {
    timer_.Stop();
    staged_result_.reset();
  }
  state_ = State::RESTARTED;
  state_transitions_.clear();
  state_transitions_.push_back(state_);

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kDelayUntilShowCalculatingMs),
               this, &BrowsingDataCounter::TransitionToShowCalculating);
  Count();
}

void BrowsingDataCounter::ReportResult(ResultInt value) {
  ReportResult(base::MakeUnique<FinishedResult>(this, value));
}

void BrowsingDataCounter::ReportResult(std::unique_ptr<Result> result) {
  DCHECK(initialized_);
  DCHECK(result->Finished());

  switch (state_) {
    case State::RESTARTED:
    case State::READY_TO_REPORT_RESULT:
      DoReportResult(std::move(result));
      return;
    case State::SHOW_CALCULATING:
      staged_result_ = std::move(result);
      return;
    case State::IDLE:
    case State::REPORT_STAGED_RESULT:
      NOTREACHED();
  }
}

void BrowsingDataCounter::DoReportResult(std::unique_ptr<Result> result) {
  DCHECK(initialized_);
  DCHECK(state_ == State::RESTARTED ||
         state_ == State::REPORT_STAGED_RESULT ||
         state_ == State::READY_TO_REPORT_RESULT);
  state_ = State::IDLE;
  state_transitions_.push_back(state_);

  timer_.Stop();
  staged_result_.reset();
  callback_.Run(std::move(result));
}

const std::vector<BrowsingDataCounter::State>&
BrowsingDataCounter::GetStateTransitionsForTesting() {
  return state_transitions_;
}

PrefService* BrowsingDataCounter::GetPrefs() const {
  return pref_service_;
}

void BrowsingDataCounter::TransitionToShowCalculating() {
  DCHECK(initialized_);
  DCHECK_EQ(State::RESTARTED, state_);
  state_ = State::SHOW_CALCULATING;
  state_transitions_.push_back(state_);

  callback_.Run(base::MakeUnique<Result>(this));
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kDelayUntilReadyToShowResultMs),
      this, &BrowsingDataCounter::TransitionToReadyToReportResult);
}

void BrowsingDataCounter::TransitionToReadyToReportResult() {
  DCHECK(initialized_);
  DCHECK_EQ(State::SHOW_CALCULATING, state_);

  if (staged_result_) {
    state_ = State::REPORT_STAGED_RESULT;
    state_transitions_.push_back(state_);
    DoReportResult(std::move(staged_result_));
  } else {
    state_ = State::READY_TO_REPORT_RESULT;
    state_transitions_.push_back(state_);
  }
}

// BrowsingDataCounter::Result -------------------------------------------------

BrowsingDataCounter::Result::Result(const BrowsingDataCounter* source)
    : source_(source) {}

BrowsingDataCounter::Result::~Result() {}

bool BrowsingDataCounter::Result::Finished() const {
  return false;
}

// BrowsingDataCounter::FinishedResult -----------------------------------------

BrowsingDataCounter::FinishedResult::FinishedResult(
    const BrowsingDataCounter* source,
    ResultInt value)
    : Result(source), value_(value) {}

BrowsingDataCounter::FinishedResult::~FinishedResult() {}

bool BrowsingDataCounter::FinishedResult::Finished() const {
  return true;
}

BrowsingDataCounter::ResultInt BrowsingDataCounter::FinishedResult::Value()
    const {
  return value_;
}

}  // namespace browsing_data
