// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_BREAKING_NEWS_METRICS_H_
#define COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_BREAKING_NEWS_METRICS_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/ntp_snippets/status.h"

namespace ntp_snippets {
namespace metrics {

void OnSubscriptionRequestCompleted(const Status& status);
void OnUnsubscriptionRequestCompleted(const Status& status);

void OnMessageReceived(bool is_handler_listening, bool contains_pushed_news);

// Received message status to report whether it contained pushed news and
// whether the handler was listening when it arrived. This enum is used in a UMA
// histogram, therefore, don't remove or reorder elements, only add new ones at
// the end (before COUNT), and keep in sync with
// ContentSuggestionsBreakingNewsMessageContainsNews in enums.xml.
enum ReceivedMessageStatus {
  WITHOUT_PUSHED_NEWS_AND_HANDLER_WAS_LISTENING = 0,
  WITH_PUSHED_NEWS_AND_HANDLER_WAS_LISTENING = 1,
  WITHOUT_PUSHED_NEWS_AND_HANDLER_WAS_NOT_LISTENING = 2,
  WITH_PUSHED_NEWS_AND_HANDLER_WAS_NOT_LISTENING = 3,
  // Insert new values here!
  COUNT
};

void OnTokenRetrieved(instance_id::InstanceID::Result result);

// |time_since_last_validation| can be absent for the first validation.
// |was_token_valid_before_validation| may be absent if it is not known (e.g. an
// error happened and a token was not received).
void OnTokenValidationAttempted(
    const base::Optional<base::TimeDelta>& time_since_last_validation,
    const base::Optional<bool>& was_token_valid_before_validation);

}  // namespace metrics
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_BREAKING_NEWS_METRICS_H_
