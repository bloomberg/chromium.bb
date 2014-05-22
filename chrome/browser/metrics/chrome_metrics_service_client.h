// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/metrics_service_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class MetricsService;

// ChromeMetricsServiceClient provides an implementation of MetricsServiceClient
// that depends on chrome/.
class ChromeMetricsServiceClient : public metrics::MetricsServiceClient,
                                   public content::NotificationObserver {
 public:
  ChromeMetricsServiceClient();
  virtual ~ChromeMetricsServiceClient();

  // metrics::MetricsServiceClient:
  virtual void SetClientID(const std::string& client_id) OVERRIDE;
  virtual bool IsOffTheRecordSessionActive() OVERRIDE;
  virtual std::string GetApplicationLocale() OVERRIDE;
  virtual bool GetBrand(std::string* brand_code) OVERRIDE;
  virtual metrics::SystemProfileProto::Channel GetChannel() OVERRIDE;
  virtual std::string GetVersionString() OVERRIDE;

  // Stores a weak pointer to the given |service|.
  // TODO(isherman): Fix the memory ownership model so that this method is not
  // needed: http://crbug.com/375248
  void set_service(MetricsService* service) { service_ = service; }

 private:
  // Registers |this| as an observer for notifications which indicate that a
  // user is performing work. This is useful to allow some features to sleep,
  // until the machine becomes active, such as precluding UMA uploads unless
  // there was recent activity.
  void RegisterForNotifications();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The MetricsService that |this| is a client of. Weak pointer.
  MetricsService* service_;

  content::NotificationRegistrar registrar_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsServiceClient);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
