// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/breaking_news_metrics.h"

#include "base/metrics/histogram_macros.h"

using instance_id::InstanceID;

namespace ntp_snippets {
namespace metrics {

namespace {

const char kHistogramSubscriptionRequestStatus[] =
    "NewTabPage.ContentSuggestions.BreakingNews.SubscriptionRequestStatus";
const char kHistogramUnsubscriptionRequestStatus[] =
    "NewTabPage.ContentSuggestions.BreakingNews.UnsubscriptionRequestStatus";

const char kHistogramMessageReceived[] =
    "NewTabPage.ContentSuggestions.BreakingNews.MessageReceived";

const char kHistogramTokenRetrievalResult[] =
    "NewTabPage.ContentSuggestions.BreakingNews.TokenRetrievalResult";

const char kHistogramTimeSinceLastTokenValidation[] =
    "NewTabPage.ContentSuggestions.BreakingNews.TimeSinceLastTokenValidation";

const char kHistogramWasTokenValidBeforeValidation[] =
    "NewTabPage.ContentSuggestions.BreakingNews.WasTokenValidBeforeValidation";

}  // namespace

void OnSubscriptionRequestCompleted(const Status& status) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramSubscriptionRequestStatus, status.code,
                            StatusCode::STATUS_CODE_COUNT);
}

void OnUnsubscriptionRequestCompleted(const Status& status) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramUnsubscriptionRequestStatus, status.code,
                            StatusCode::STATUS_CODE_COUNT);
}

void OnMessageReceived(bool contains_pushed_news) {
  UMA_HISTOGRAM_BOOLEAN(kHistogramMessageReceived, contains_pushed_news);
}

void OnTokenRetrieved(InstanceID::Result result) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramTokenRetrievalResult, result,
                            InstanceID::Result::LAST_RESULT + 1);
}

void OnTokenValidationAttempted(
    const base::Optional<base::TimeDelta>& time_since_last_validation,
    const base::Optional<bool>& was_token_valid_before_validation) {
  if (time_since_last_validation.has_value()) {
    UMA_HISTOGRAM_CUSTOM_TIMES(kHistogramTimeSinceLastTokenValidation,
                               *time_since_last_validation,
                               /*min=*/base::TimeDelta::FromSeconds(1),
                               /*max=*/base::TimeDelta::FromDays(7),
                               /*bucket_count=*/50);
  }
  if (was_token_valid_before_validation.has_value()) {
    UMA_HISTOGRAM_BOOLEAN(kHistogramWasTokenValidBeforeValidation,
                          *was_token_valid_before_validation);
  }
}

}  // namespace metrics
}  // namespace ntp_snippets
