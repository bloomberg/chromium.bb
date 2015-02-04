// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_METRICS_HELPER_H_
#define CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_METRICS_HELPER_H_

#include <string>

#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace extensions {
class ExperienceSamplingEvent;
}

// Most of the security interstitials share a common layout and set of
// choices. SecurityInterstitialMetricsHelper is intended to help the security
// interstitials record user choices in a common way via METRICS histograms
// and RAPPOR metrics.
class SecurityInterstitialMetricsHelper {
 public:
  // These enums are used for histograms.  Don't reorder, delete, or insert
  // elements. New elements should be added at the end (right before the max).
  enum SecurityInterstitialDecision {
    SHOW,
    PROCEED,
    DONT_PROCEED,
    PROCEEDING_DISABLED,
    MAX_DECISION
  };
  enum SecurityInterstitialInteraction {
    TOTAL_VISITS,
    SHOW_ADVANCED,
    SHOW_PRIVACY_POLICY,
    SHOW_DIAGNOSTIC,
    SHOW_LEARN_MORE,
    RELOAD,
    OPEN_TIME_SETTINGS,
    MAX_INTERACTION
  };

  enum RapporReporting {
    REPORT_RAPPOR,
    SKIP_RAPPOR,
  };

  // Args:
  //   url: URL of page that triggered the interstitial. Only origin is used.
  //   uma_prefix: Histogram prefix for UMA.
  //               examples: "phishing", "ssl_overridable"
  //   rappor_prefix: Metric prefix for Rappor.
  //               examples: "phishing", "ssl"
  //   rappor_reporting: Used to skip rappor rapporting if desired.
  //   sampling_event_name: Event name for Experience Sampling.
  //                        e.g. "phishing_interstitial_"
  SecurityInterstitialMetricsHelper(content::WebContents* web_contents,
                                    const GURL& url,
                                    const std::string& uma_prefix,
                                    const std::string& rappor_prefix,
                                    RapporReporting rappor_reporting,
                                    const std::string& sampling_event_name);
  ~SecurityInterstitialMetricsHelper();

  // Record a user decision or interaction to the appropriate UMA histogram
  // and potentially in a RAPPOR metric.
  void RecordUserDecision(SecurityInterstitialDecision decision);
  void RecordUserInteraction(SecurityInterstitialInteraction interaction);

 private:
  // Used to query the HistoryService to see if the URL is in history.
  void OnGotHistoryCount(bool success, int num_visits, base::Time first_visit);

  content::WebContents* web_contents_;
  const GURL request_url_;
  const std::string uma_prefix_;
  const std::string rappor_prefix_;
  const RapporReporting rappor_reporting_;
  const std::string sampling_event_name_;
  int num_visits_;
  base::CancelableTaskTracker request_tracker_;
#if defined(ENABLE_EXTENSIONS)
  scoped_ptr<extensions::ExperienceSamplingEvent> sampling_event_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SecurityInterstitialMetricsHelper);
};

#endif  // CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_METRICS_HELPER_H_
