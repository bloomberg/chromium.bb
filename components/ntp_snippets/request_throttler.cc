// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/request_throttler.h"

#include <vector>

#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

namespace {

// Enumeration listing all possible outcomes for fetch attempts. Used for UMA
// histogram, so do not change existing values. Insert new values at the end,
// and update the histogram definition.
enum class RequestStatus {
  FORCED,
  QUOTA_GRANTED,
  QUOTA_EXCEEDED,
  REQUEST_STATUS_COUNT
};

}  // namespace

struct RequestThrottler::RequestTypeInfo {
    const char* name;
    const char* count_pref;
    const char* day_pref;
    const int default_quota;
};

// When adding a new type here, extend also the "RequestCounterTypes"
// <histogram_suffixes> in histograms.xml with the |name| string.
const RequestThrottler::RequestTypeInfo RequestThrottler::kRequestTypeInfo[] = {
    // RequestCounter::RequestType::CONTENT_SUGGESTION_FETCHER,
    {"SuggestionFetcher", prefs::kSnippetFetcherQuotaCount,
     prefs::kSnippetFetcherQuotaDay, 50}};

RequestThrottler::RequestThrottler(PrefService* pref_service, RequestType type)
    : pref_service_(pref_service),
      type_info_(kRequestTypeInfo[static_cast<int>(type)]) {
  DCHECK(pref_service);

  std::string quota = variations::GetVariationParamValue(
      ntp_snippets::kStudyName,
      base::StringPrintf("quota_%s", GetRequestTypeAsString()));
  if (!base::StringToInt(quota, &quota_)) {
    LOG_IF(WARNING, !quota.empty())
        << "Invalid variation parameter for quota for "
        << GetRequestTypeAsString();
    quota_ = type_info_.default_quota;
  }

  // Since the histogram names are dynamic, we cannot use the standard macros
  // and we need to lookup the histograms, instead.
  int status_count = static_cast<int>(RequestStatus::REQUEST_STATUS_COUNT);
  // Corresponds to UMA_HISTOGRAM_ENUMERATION(name, sample, |status_count|).
  histogram_request_status_ = base::LinearHistogram::FactoryGet(
      base::StringPrintf("NewTabPage.RequestThrottler.RequestStatus_%s",
                         GetRequestTypeAsString()),
      1, status_count, status_count + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  // Corresponds to UMA_HISTOGRAM_COUNTS_100(name, sample).
  histogram_per_day_ = base::Histogram::FactoryGet(
      base::StringPrintf("NewTabPage.RequestThrottler.PerDay_%s",
                         GetRequestTypeAsString()),
      1, 100, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
}

// static
void RequestThrottler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  for (const RequestTypeInfo& info : kRequestTypeInfo) {
    registry->RegisterIntegerPref(info.count_pref, 0);
    registry->RegisterIntegerPref(info.day_pref, 0);
  }
}

bool RequestThrottler::DemandQuotaForRequest(bool forced_request) {
  ResetCounterIfDayChanged();

  if (forced_request) {
    histogram_request_status_->Add(static_cast<int>(RequestStatus::FORCED));
    return true;
  }

  int new_count = GetCount() + 1;
  SetCount(new_count);
  bool available = (new_count <= quota_);

  histogram_request_status_->Add(
      static_cast<int>(available ? RequestStatus::QUOTA_GRANTED
                                 : RequestStatus::QUOTA_EXCEEDED));
  return available;
}

void RequestThrottler::ResetCounterIfDayChanged() {
  // The count of days since a fixed reference (the Unix epoch).
  int now_day = (base::Time::Now() - base::Time::UnixEpoch()).InDays();

  if (!HasDay()) {
    // The counter is used for the first time in this profile.
    SetDay(now_day);
  } else if (now_day != GetDay()) {
    // Day has changed - report the number of requests from the previous day.
    histogram_per_day_->Add(GetCount());
    // Reset the counter.
    SetCount(0);
    SetDay(now_day);
  }
}

const char* RequestThrottler::GetRequestTypeAsString() const {
  return type_info_.name;
}

int RequestThrottler::GetCount() const {
  return pref_service_->GetInteger(type_info_.count_pref);
}

void RequestThrottler::SetCount(int count) {
  pref_service_->SetInteger(type_info_.count_pref, count);
}

int RequestThrottler::GetDay() const {
  return pref_service_->GetInteger(type_info_.day_pref);
}

void RequestThrottler::SetDay(int day) {
  pref_service_->SetInteger(type_info_.day_pref, day);
}

bool RequestThrottler::HasDay() const {
  return pref_service_->HasPrefPath(type_info_.day_pref);
}

}  // namespace ntp_snippets
