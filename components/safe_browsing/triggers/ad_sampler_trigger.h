// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_SAMPLER_TRIGGER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_SAMPLER_TRIGGER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace content {
class NavigationHandle;
}

namespace history {
class HistoryService;
}

namespace net {
class URLRequestContextGetter;
}

namespace safe_browsing {
class TriggerManager;

// Param name of the denominator for controlling sampling frequency.
extern const char kAdSamplerFrequencyDenominatorParam[];

// A frequency denominator with this value indicates sampling is disabled.
extern const size_t kSamplerFrequencyDisabled;

// Metric for tracking what the Ad Sampler trigger does on each navigation.
extern const char kAdSamplerTriggerActionMetricName[];

// Actions performed by this trigger. These values are written to logs. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum AdSamplerTriggerAction {
  // An event occurred that caused the trigger to perform its checks.
  TRIGGER_CHECK = 0,
  // An ad was detected and a sample was collected.
  AD_SAMPLED = 1,
  // An ad was detected but no sample was taken to honour sampling frequency.
  NO_SAMPLE_AD_SKIPPED_FOR_FREQUENCY = 2,
  // No ad was detected.
  NO_SAMPLE_NO_AD = 3,
  // An ad was detected and could have been sampled, but the trigger manager
  // rejected the report (eg: because a report was already in progress).
  NO_SAMPLE_COULD_NOT_START_REPORT = 4,
  // New actions must be added before MAX_ACTIONS.
  MAX_ACTIONS
};

// This class periodically checks for Google ads on the page and may decide to
// send a report to Google with the ad's structure for further analysis.
class AdSamplerTrigger : public content::WebContentsObserver,
                         public content::WebContentsUserData<AdSamplerTrigger> {
 public:
  ~AdSamplerTrigger() override;

  static void CreateForWebContents(
      content::WebContents* web_contents,
      TriggerManager* trigger_manager,
      PrefService* prefs,
      net::URLRequestContextGetter* request_context,
      history::HistoryService* history_service);

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class AdSamplerTriggerTest;
  friend class content::WebContentsUserData<AdSamplerTrigger>;
  FRIEND_TEST_ALL_PREFIXES(AdSamplerTriggerTestFinch,
                           FrequencyDenominatorFeature);

  AdSamplerTrigger(content::WebContents* web_contents,
                   TriggerManager* trigger_manager,
                   PrefService* prefs,
                   net::URLRequestContextGetter* request_context,
                   history::HistoryService* history_service);

  // Ad samples will be collected with frequency
  // 1/|sampler_frequency_denominator_|
  size_t sampler_frequency_denominator_;

  // TriggerManager gets called if this trigger detects an ad and wants to
  // collect some data about it. Not owned.
  TriggerManager* trigger_manager_;

  PrefService* prefs_;
  net::URLRequestContextGetter* request_context_;
  history::HistoryService* history_service_;

  DISALLOW_COPY_AND_ASSIGN(AdSamplerTrigger);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_SAMPLER_TRIGGER_H_
