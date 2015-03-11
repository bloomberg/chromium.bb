// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/security_interstitial_metrics_helper.h"

#include <string>

#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/rappor/rappor_service.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#endif

SecurityInterstitialMetricsHelper::SecurityInterstitialMetricsHelper(
    content::WebContents* web_contents,
    const GURL& request_url,
    const std::string& uma_prefix,
    const std::string& rappor_prefix,
    RapporReporting rappor_reporting,
    const std::string& sampling_event_name)
    : web_contents_(web_contents),
      request_url_(request_url),
      uma_prefix_(uma_prefix),
      rappor_prefix_(rappor_prefix),
      rappor_reporting_(rappor_reporting),
      sampling_event_name_(sampling_event_name),
      num_visits_(-1) {
  DCHECK(!uma_prefix_.empty());
  DCHECK(!rappor_prefix_.empty());
  DCHECK(!sampling_event_name_.empty());
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service) {
    history_service->GetVisibleVisitCountToHost(
        request_url_,
        base::Bind(&SecurityInterstitialMetricsHelper::OnGotHistoryCount,
                   base::Unretained(this)),
        &request_tracker_);
  }
}

SecurityInterstitialMetricsHelper::~SecurityInterstitialMetricsHelper() {
}

// Directly adds to the UMA histograms, using the same properties as
// UMA_HISTOGRAM_ENUMERATION, because the macro doesn't allow non-constant
// histogram names.  Reports to Rappor for certain decisions.
void SecurityInterstitialMetricsHelper::RecordUserDecision(
    SecurityInterstitialDecision decision) {
  // UMA
  std::string decision_histogram_name(
      "interstitial." + uma_prefix_ + ".decision");
  base::HistogramBase* decision_histogram = base::LinearHistogram::FactoryGet(
      decision_histogram_name, 1, MAX_DECISION, MAX_DECISION + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  decision_histogram->Add(decision);

  // Rappor
  rappor::RapporService* rappor_service = g_browser_process->rappor_service();
  if (rappor_service && rappor_reporting_ == REPORT_RAPPOR &&
      (decision == PROCEED || decision == DONT_PROCEED)) {
    // |domain| will be empty for hosts w/o TLDs
    const std::string domain = rappor::GetDomainAndRegistrySampleFromGURL(
        request_url_);

    // e.g. "interstitial.malware.domain" or "interstitial.ssl.domain"
    const std::string metric_name =
        "interstitial." + rappor_prefix_ + ".domain";
    rappor_service->RecordSample(metric_name, rappor::COARSE_RAPPOR_TYPE,
                                 domain);
    // TODO(nparker): Add reporting of (num_visits > 0) and decision
    // once http://crbug.com/451647 is fixed.
  }

#if defined(ENABLE_EXTENSIONS)
  if (!sampling_event_.get()) {
    sampling_event_.reset(new extensions::ExperienceSamplingEvent(
        sampling_event_name_,
        request_url_,
        web_contents_->GetLastCommittedURL(),
        web_contents_->GetBrowserContext()));
  }
  switch (decision) {
    case PROCEED:
      sampling_event_->CreateUserDecisionEvent(
          extensions::ExperienceSamplingEvent::kProceed);
      break;
    case DONT_PROCEED:
      sampling_event_->CreateUserDecisionEvent(
          extensions::ExperienceSamplingEvent::kDeny);
      break;
    case SHOW:
    case PROCEEDING_DISABLED:
    case MAX_DECISION:
      break;
  }
#endif

  // Record additional information about sites that users have
  // visited before.
  if (num_visits_ < 1 || (decision != PROCEED && decision != DONT_PROCEED))
    return;
  std::string history_histogram_name(
      "interstitial." + uma_prefix_ + ".decision.repeat_visit");
  base::HistogramBase* history_histogram = base::LinearHistogram::FactoryGet(
      history_histogram_name, 1, MAX_DECISION, MAX_DECISION + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  history_histogram->Add(SHOW);
  history_histogram->Add(decision);
}

void SecurityInterstitialMetricsHelper::RecordUserInteraction(
    SecurityInterstitialInteraction interaction) {
  std::string interaction_histogram_name(
      "interstitial." + uma_prefix_ + ".interaction");
  base::HistogramBase* interaction_histogram =
      base::LinearHistogram::FactoryGet(
          interaction_histogram_name, 1, MAX_INTERACTION, MAX_INTERACTION + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  interaction_histogram->Add(interaction);

#if defined(ENABLE_EXTENSIONS)
  if (!sampling_event_.get()) {
    sampling_event_.reset(new extensions::ExperienceSamplingEvent(
        sampling_event_name_,
        request_url_,
        web_contents_->GetLastCommittedURL(),
        web_contents_->GetBrowserContext()));
  }
  switch (interaction) {
    case SHOW_LEARN_MORE:
      sampling_event_->set_has_viewed_learn_more(true);
      break;
    case SHOW_ADVANCED:
      sampling_event_->set_has_viewed_details(true);
      break;
    case SHOW_PRIVACY_POLICY:
    case SHOW_DIAGNOSTIC:
    case RELOAD:
    case OPEN_TIME_SETTINGS:
    case TOTAL_VISITS:
    case MAX_INTERACTION:
      break;
  }
#endif
}

void SecurityInterstitialMetricsHelper::OnGotHistoryCount(
    bool success,
    int num_visits,
    base::Time first_visit) {
  if (success)
    num_visits_ = num_visits;
}
