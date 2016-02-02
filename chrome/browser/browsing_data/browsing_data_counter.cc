// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_counter.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

BrowsingDataCounter::BrowsingDataCounter() {}

BrowsingDataCounter::~BrowsingDataCounter() {
}

void BrowsingDataCounter::Init(
    Profile* profile,
    const Callback& callback) {
  DCHECK(!initialized_);
  profile_ = profile;
  callback_ = callback;
  pref_.Init(
      GetPrefName(),
      profile_->GetPrefs(),
      base::Bind(&BrowsingDataCounter::Restart,
                 base::Unretained(this)));
  period_.Init(
      prefs::kDeleteTimePeriod,
      profile_->GetPrefs(),
      base::Bind(&BrowsingDataCounter::Restart,
                 base::Unretained(this)));

  initialized_ = true;
  OnInitialized();
}

Profile* BrowsingDataCounter::GetProfile() const {
  return profile_;
}

void BrowsingDataCounter::OnInitialized() {
}

base::Time BrowsingDataCounter::GetPeriodStart() {
  return BrowsingDataRemover::CalculateBeginDeleteTime(
      static_cast<BrowsingDataRemover::TimePeriod>(*period_));
}

void BrowsingDataCounter::Restart() {
  DCHECK(initialized_);

  // If this data type was unchecked for deletion, we do not need to count it.
  if (!profile_->GetPrefs()->GetBoolean(GetPrefName()))
    return;

  callback_.Run(make_scoped_ptr(new Result(this)));

  Count();
}

void BrowsingDataCounter::ReportResult(ResultInt value) {
  DCHECK(initialized_);
  callback_.Run(make_scoped_ptr(new FinishedResult(this, value)));
}

void BrowsingDataCounter::ReportResult(scoped_ptr<Result> result) {
  DCHECK(initialized_);
  callback_.Run(std::move(result));
}

// BrowsingDataCounter::Result -------------------------------------------------

BrowsingDataCounter::Result::Result(const BrowsingDataCounter* source)
    : source_(source) {
}

BrowsingDataCounter::Result::~Result() {
}

bool BrowsingDataCounter::Result::Finished() const {
  return false;
}

// BrowsingDataCounter::FinishedResult -----------------------------------------

BrowsingDataCounter::FinishedResult::FinishedResult(
    const BrowsingDataCounter* source, ResultInt value)
    : Result(source),
      value_(value) {
}

BrowsingDataCounter::FinishedResult::~FinishedResult() {
}

bool BrowsingDataCounter::FinishedResult::Finished() const {
  return true;
}

BrowsingDataCounter::ResultInt
    BrowsingDataCounter::FinishedResult::Value() const {
  return value_;
}
