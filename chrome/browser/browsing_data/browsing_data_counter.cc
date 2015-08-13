// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_counter.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"

BrowsingDataCounter::BrowsingDataCounter()
    : counting_(false),
      initialized_(false) {
}

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
      base::Bind(&BrowsingDataCounter::RestartCounting,
                 base::Unretained(this)));

  initialized_ = true;
  OnInitialized();
  RestartCounting();
}

Profile* BrowsingDataCounter::GetProfile() const {
  return profile_;
}

void BrowsingDataCounter::OnInitialized() {
}

void BrowsingDataCounter::RestartCounting() {
  DCHECK(initialized_);

  // If this data type was unchecked for deletion, we do not need to count it.
  if (!profile_->GetPrefs()->GetBoolean(GetPrefName()))
    return;

  // If counting is already in progress, do not restart it.
  if (counting_)
    return;

  callback_.Run(false, 0u);

  counting_ = true;
  Count();
}

void BrowsingDataCounter::ReportResult(uint32 value) {
  DCHECK(initialized_);
  DCHECK(counting_);
  counting_ = false;
  callback_.Run(true, value);
}
