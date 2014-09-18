// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_STABILITY_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CHROME_STABILITY_METRICS_PROVIDER_H_

#include "base/basictypes.h"
#include "base/metrics/user_metrics.h"
#include "base/process/kill.h"
#include "components/metrics/metrics_provider.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefRegistrySimple;

namespace content {
class RenderProcessHost;
class WebContents;
}

// ChromeStabilityMetricsProvider gathers and logs Chrome-specific stability-
// related metrics.
class ChromeStabilityMetricsProvider
    : public metrics::MetricsProvider,
      public content::BrowserChildProcessObserver,
      public content::NotificationObserver {
 public:
  ChromeStabilityMetricsProvider();
  virtual ~ChromeStabilityMetricsProvider();

  // metrics::MetricsDataProvider:
  virtual void OnRecordingEnabled() OVERRIDE;
  virtual void OnRecordingDisabled() OVERRIDE;
  virtual void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) OVERRIDE;
  virtual void ClearSavedStabilityMetrics() OVERRIDE;

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // content::BrowserChildProcessObserver:
  virtual void BrowserChildProcessCrashed(
      const content::ChildProcessData& data) OVERRIDE;

  // Logs the initiation of a page load and uses |web_contents| to do
  // additional logging of the type of page loaded.
  void LogLoadStarted(content::WebContents* web_contents);

  // Records a renderer process crash.
  void LogRendererCrash(content::RenderProcessHost* host,
                        base::TerminationStatus status,
                        int exit_code);

  // Records a renderer process hang.
  void LogRendererHang();

  // Registrar for receiving stability-related notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStabilityMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_STABILITY_METRICS_PROVIDER_H_
