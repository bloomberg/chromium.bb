// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_OMNIBOX_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_OMNIBOX_METRICS_PROVIDER_H_

#include "base/basictypes.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

struct OmniboxLog;

// OmniboxMetricsProvider is responsible for filling out the |omnibox_event|
// section of the UMA proto.
class OmniboxMetricsProvider : public metrics::MetricsProvider,
                               public content::NotificationObserver {
 public:
  OmniboxMetricsProvider();
  virtual ~OmniboxMetricsProvider();

  // metrics::MetricsDataProvider:
  virtual void OnRecordingEnabled() OVERRIDE;
  virtual void OnRecordingDisabled() OVERRIDE;
  virtual void ProvideGeneralMetrics(
      metrics::ChromeUserMetricsExtension* uma_proto) OVERRIDE;

 private:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Records the input text, available choices, and selected entry when the
  // user uses the Omnibox to open a URL.
  void RecordOmniboxOpenedURL(const OmniboxLog& log);

  // Registar for receiving Omnibox event notifications.
  content::NotificationRegistrar registrar_;

  // Saved cache of generated Omnibox event protos, to be copied into the UMA
  // proto when ProvideGeneralMetrics() is called.
  metrics::ChromeUserMetricsExtension omnibox_events_cache;

  DISALLOW_COPY_AND_ASSIGN(OmniboxMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_OMNIBOX_METRICS_PROVIDER_H_
