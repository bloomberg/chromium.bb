// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_UMA_HELPER_H_
#define CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_UMA_HELPER_H_

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
// choices. SecurityInterstitialUmaHelper is intended to help the security
// interstitials record user choices in a common way via UMA histograms.
class SecurityInterstitialUmaHelper {
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

  SecurityInterstitialUmaHelper(content::WebContents* web_contents,
                                const GURL& url,
                                const std::string& histogram_prefix,
                                const std::string& sampling_event_name);
  ~SecurityInterstitialUmaHelper();

  // Record a user decision or interaction to the appropriate UMA histogram.
  void RecordUserDecision(SecurityInterstitialDecision decision);
  void RecordUserInteraction(SecurityInterstitialInteraction interaction);

 private:
  // Used to query the HistoryService to see if the URL is in history.
  void OnGotHistoryCount(bool success, int num_visits, base::Time first_visit);

  content::WebContents* web_contents_;
  const GURL request_url_;
  const std::string histogram_prefix_;
  const std::string sampling_event_name_;
  int num_visits_;
  base::CancelableTaskTracker request_tracker_;
#if defined(ENABLE_EXTENSIONS)
  scoped_ptr<extensions::ExperienceSamplingEvent> sampling_event_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SecurityInterstitialUmaHelper);
};

#endif  // CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_UMA_HELPER_H_
