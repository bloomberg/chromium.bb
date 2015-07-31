// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_CHROME_METRICS_HELPER_H_
#define CHROME_BROWSER_INTERSTITIALS_CHROME_METRICS_HELPER_H_

#include <string>

#include "components/security_interstitials/core/metrics_helper.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace extensions {
class ExperienceSamplingEvent;
}

// This class adds desktop-Chrome-specific metrics (extension experience
// sampling) to the security_interstitials::MetricsHelper. Together, they
// record UMA, Rappor, and experience sampling metrics.
class ChromeMetricsHelper : public security_interstitials::MetricsHelper {
 public:
  ChromeMetricsHelper(
      content::WebContents* web_contents,
      const GURL& url,
      const security_interstitials::MetricsHelper::ReportDetails settings,
      const std::string& sampling_event_name);
  ~ChromeMetricsHelper() override;

 protected:
  // security_interstitials::MetricsHelper methods:
  void RecordExtraUserDecisionMetrics(
      security_interstitials::MetricsHelper::Decision decision) override;
  void RecordExtraUserInteractionMetrics(
      security_interstitials::MetricsHelper::Interaction interaction) override;

 private:
  content::WebContents* web_contents_;
  const GURL request_url_;
  const std::string sampling_event_name_;
#if defined(ENABLE_EXTENSIONS)
  scoped_ptr<extensions::ExperienceSamplingEvent> sampling_event_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsHelper);
};

#endif  // CHROME_BROWSER_INTERSTITIALS_CHROME_METRICS_HELPER_H_
