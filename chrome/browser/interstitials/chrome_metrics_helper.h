// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_CHROME_METRICS_HELPER_H_
#define CHROME_BROWSER_INTERSTITIALS_CHROME_METRICS_HELPER_H_

#include <string>

#include "base/macros.h"
#include "chrome/common/features.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "extensions/features/features.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace extensions {
class ExperienceSamplingEvent;
}

class CaptivePortalMetricsRecorder;

// This class adds desktop-Chrome-specific metrics (extension experience
// sampling) to the security_interstitials::MetricsHelper. Together, they
// record UMA, and experience sampling metrics.

// This class is meant to be used on the UI thread for captive portal metrics.
class ChromeMetricsHelper : public security_interstitials::MetricsHelper {
 public:
  ChromeMetricsHelper(
      content::WebContents* web_contents,
      const GURL& url,
      const security_interstitials::MetricsHelper::ReportDetails settings,
      const std::string& sampling_event_name);
  ~ChromeMetricsHelper() override;

  void StartRecordingCaptivePortalMetrics(bool overridable);

 protected:
  // security_interstitials::MetricsHelper methods:
  void RecordExtraUserDecisionMetrics(
      security_interstitials::MetricsHelper::Decision decision) override;
  void RecordExtraUserInteractionMetrics(
      security_interstitials::MetricsHelper::Interaction interaction) override;
  void RecordExtraShutdownMetrics() override;

 private:
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION) || BUILDFLAG(ENABLE_EXTENSIONS)
  content::WebContents* web_contents_;
#endif
  const GURL request_url_;
  const std::string sampling_event_name_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<extensions::ExperienceSamplingEvent> sampling_event_;
#endif
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  std::unique_ptr<CaptivePortalMetricsRecorder> captive_portal_recorder_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsHelper);
};

#endif  // CHROME_BROWSER_INTERSTITIALS_CHROME_METRICS_HELPER_H_
