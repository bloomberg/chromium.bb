// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_SAMPLER_TRIGGER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_SAMPLER_TRIGGER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}

namespace safe_browsing {

// Param name of the denominator for controlling sampling frequency.
extern const char kAdSamplerFrequencyDenominatorParam[];

// A frequency denominator with this value indicates sampling is disabled.
extern const size_t kSamplerFrequencyDisabled;

// This class periodically checks for Google ads on the page and may decide to
// send a report to Google with the ad's structure for further analysis.
class AdSamplerTrigger : public content::WebContentsObserver,
                         public content::WebContentsUserData<AdSamplerTrigger> {
 public:
  ~AdSamplerTrigger() override;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit AdSamplerTrigger(content::WebContents* contents);
  friend class content::WebContentsUserData<AdSamplerTrigger>;

  // Ad samples will be collected with frequency
  // 1/|sampler_frequency_denominator_|
  size_t sampler_frequency_denominator_;

  DISALLOW_COPY_AND_ASSIGN(AdSamplerTrigger);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_SAMPLER_TRIGGER_H_
