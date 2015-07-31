// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/metrics_helper.h"

#include "base/metrics/histogram.h"
#include "components/history/core/browser/history_service.h"
#include "components/rappor/rappor_service.h"
#include "components/rappor/rappor_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace {
// Used for setting bits in Rappor's "interstitial.*.flags"
enum InterstitialFlagBits {
  DID_PROCEED = 0,
  IS_REPEAT_VISIT = 1,
  HIGHEST_USED_BIT = 1
};
}  // namespace

namespace security_interstitials {

MetricsHelper::MetricsHelper(const GURL& request_url,
                             const ReportDetails settings,
                             history::HistoryService* history_service,
                             rappor::RapporService* rappor_service)
    : request_url_(request_url),
      settings_(settings),
      rappor_service_(rappor_service),
      num_visits_(-1) {
  DCHECK(!settings_.metric_prefix.empty());
  if (settings_.rappor_report_type == rappor::NUM_RAPPOR_TYPES)  // Default.
    rappor_service_ = nullptr;
  DCHECK(!rappor_service_ || !settings_.rappor_prefix.empty());
  if (history_service) {
    history_service->GetVisibleVisitCountToHost(
        request_url_,
        base::Bind(&MetricsHelper::OnGotHistoryCount, base::Unretained(this)),
        &request_tracker_);
  }
}

// Directly adds to the UMA histograms, using the same properties as
// UMA_HISTOGRAM_ENUMERATION, because the macro doesn't allow non-constant
// histogram names. Reports to Rappor for certain decisions.
void MetricsHelper::RecordUserDecision(Decision decision) {
  // UMA
  const std::string decision_histogram_name(
      "interstitial." + settings_.metric_prefix + ".decision");
  base::HistogramBase* decision_histogram = base::LinearHistogram::FactoryGet(
      decision_histogram_name, 1, MAX_DECISION, MAX_DECISION + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  decision_histogram->Add(decision);

  // Rappor
  if (rappor_service_ && (decision == PROCEED || decision == DONT_PROCEED)) {
    scoped_ptr<rappor::Sample> sample =
        rappor_service_->CreateSample(settings_.rappor_report_type);

    // This will populate, for example, "intersitial.malware.domain" or
    // "interstitial.ssl2.domain".  |domain| will be empty for hosts w/o TLDs.
    const std::string domain =
        rappor::GetDomainAndRegistrySampleFromGURL(request_url_);
    sample->SetStringField("domain", domain);

    // Only report history and decision if we have history data.
    if (num_visits_ >= 0) {
      int flags = 0;
      if (decision == PROCEED)
        flags |= 1 << InterstitialFlagBits::DID_PROCEED;
      if (num_visits_ > 0)
        flags |= 1 << InterstitialFlagBits::IS_REPEAT_VISIT;
      // e.g. "interstitial.malware.flags"
      sample->SetFlagsField("flags", flags,
                            InterstitialFlagBits::HIGHEST_USED_BIT + 1);
    }
    rappor_service_->RecordSampleObj("interstitial." + settings_.rappor_prefix,
                                     sample.Pass());
  }

  // Record additional information about sites that users have
  // visited before.
  if (num_visits_ < 1)
    return;
  std::string history_histogram_name("interstitial." + settings_.metric_prefix +
                                     ".decision.repeat_visit");
  base::HistogramBase* history_histogram = base::LinearHistogram::FactoryGet(
      history_histogram_name, 1, MAX_DECISION, MAX_DECISION + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  history_histogram->Add(SHOW);
  history_histogram->Add(decision);

  RecordExtraUserDecisionMetrics(decision);
}

void MetricsHelper::RecordUserInteraction(Interaction interaction) {
  const std::string interaction_histogram_name(
      "interstitial." + settings_.metric_prefix + ".interaction");
  base::HistogramBase* interaction_histogram =
      base::LinearHistogram::FactoryGet(
          interaction_histogram_name, 1, MAX_INTERACTION, MAX_INTERACTION + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  interaction_histogram->Add(interaction);

  RecordExtraUserInteractionMetrics(interaction);
}

void MetricsHelper::OnGotHistoryCount(bool success,
                                      int num_visits,
                                      base::Time /*first_visit*/) {
  if (success)
    num_visits_ = num_visits;
}

}  // namespace security_interstitials
