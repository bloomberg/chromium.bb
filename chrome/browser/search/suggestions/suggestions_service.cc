// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_service.h"

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/metrics/variations/variations_http_header_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace suggestions {

namespace {

// Used to UMA log the state of the last response from the server.
enum SuggestionsResponseState {
  RESPONSE_EMPTY,
  RESPONSE_INVALID,
  RESPONSE_VALID,
  RESPONSE_STATE_SIZE
};

// Will log the supplied response |state|.
void LogResponseState(SuggestionsResponseState state) {
  UMA_HISTOGRAM_ENUMERATION("Suggestions.ResponseState", state,
                            RESPONSE_STATE_SIZE);
}

// Obtains the experiment parameter under the supplied |key|.
std::string GetExperimentParam(const std::string& key) {
  return chrome_variations::GetVariationParamValue(kSuggestionsFieldTrialName,
                                                   key);
}

}  // namespace

const char kSuggestionsFieldTrialName[] = "ChromeSuggestions";
const char kSuggestionsFieldTrialURLParam[] = "url";
const char kSuggestionsFieldTrialStateParam[] = "state";
const char kSuggestionsFieldTrialStateEnabled[] = "enabled";

SuggestionsService::SuggestionsService(Profile* profile)
    : profile_(profile) {
  // Obtain the URL to use to fetch suggestions data from the Variations param.
  suggestions_url_ = GURL(GetExperimentParam(kSuggestionsFieldTrialURLParam));
}

SuggestionsService::~SuggestionsService() {
}

// static
bool SuggestionsService::IsEnabled() {
  return GetExperimentParam(kSuggestionsFieldTrialStateParam) ==
      kSuggestionsFieldTrialStateEnabled;
}

void SuggestionsService::FetchSuggestionsData() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  pending_request_.reset(net::URLFetcher::Create(
      0, suggestions_url_, net::URLFetcher::GET, this));
  pending_request_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  pending_request_->SetRequestContext(profile_->GetRequestContext());
  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  chrome_variations::VariationsHttpHeaderProvider::GetInstance()->AppendHeaders(
      pending_request_->GetOriginalURL(), profile_->IsOffTheRecord(), false,
      &headers);
  pending_request_->SetExtraRequestHeaders(headers.ToString());
  pending_request_->Start();

  last_request_started_time_ = base::TimeTicks::Now();
}

void SuggestionsService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(pending_request_.get(), source);

  // The fetcher will be deleted when the request is handled.
  scoped_ptr<const net::URLFetcher> request(pending_request_.release());
  const net::URLRequestStatus& request_status = request->GetStatus();
  if (request_status.status() != net::URLRequestStatus::SUCCESS) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Suggestions.FailedRequestErrorCode",
                                -request_status.error());
    DVLOG(1) << "Suggestions server request failed with error: "
             << request_status.error() << ": "
             << net::ErrorToString(request_status.error());
    return;
  }

  // Log the response code.
  const int response_code = request->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY("Suggestions.FetchResponseCode",
                              response_code);

  const base::TimeDelta latency =
      base::TimeTicks::Now() - last_request_started_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES("Suggestions.FetchSuccessLatency", latency);

  std::string suggestions_data;
  bool success = request->GetResponseAsString(&suggestions_data);

  DCHECK(success);
  if (suggestions_data.empty()) {
    LogResponseState(RESPONSE_EMPTY);
  } else if (suggestions_.ParseFromString(suggestions_data)) {
    LogResponseState(RESPONSE_VALID);
  } else {
    LogResponseState(RESPONSE_INVALID);
  }
}

}  // namespace suggestions
