// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_METRICS_CAST_METRICS_SERVICE_CLIENT_H_
#define CHROMECAST_METRICS_CAST_METRICS_SERVICE_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/metrics/metrics_service_client.h"

class PrefService;

namespace base {
class TaskRunner;
}

namespace metrics {
class MetricsService;
class MetricsStateManager;
}  // namespace metrics

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace chromecast {
namespace metrics {

class CastMetricsServiceClient : public ::metrics::MetricsServiceClient {
 public:
  virtual ~CastMetricsServiceClient();

  static CastMetricsServiceClient* Create(
      base::TaskRunner* io_task_runner,
      PrefService* pref_service,
      net::URLRequestContextGetter* request_context);

  // metrics::MetricsServiceClient implementation:
  virtual void SetMetricsClientId(const std::string& client_id) OVERRIDE;
  virtual bool IsOffTheRecordSessionActive() OVERRIDE;
  virtual std::string GetApplicationLocale() OVERRIDE;
  virtual bool GetBrand(std::string* brand_code) OVERRIDE;
  virtual ::metrics::SystemProfileProto::Channel GetChannel() OVERRIDE;
  virtual std::string GetVersionString() OVERRIDE;
  virtual void OnLogUploadComplete() OVERRIDE;
  virtual void StartGatheringMetrics(
      const base::Closure& done_callback) OVERRIDE;
  virtual void CollectFinalMetrics(const base::Closure& done_callback) OVERRIDE;
  virtual scoped_ptr< ::metrics::MetricsLogUploader> CreateUploader(
      const std::string& server_url,
      const std::string& mime_type,
      const base::Callback<void(int)>& on_upload_complete) OVERRIDE;

  // Starts/stops the metrics service.
  void EnableMetricsService(bool enabled);

 private:
  CastMetricsServiceClient(
      base::TaskRunner* io_task_runner,
      PrefService* pref_service,
      net::URLRequestContextGetter* request_context);

  // Returns whether or not metrics reporting is enabled.
  bool IsReportingEnabled();

  scoped_ptr< ::metrics::MetricsStateManager> metrics_state_manager_;
  scoped_ptr< ::metrics::MetricsService> metrics_service_;
  net::URLRequestContextGetter* request_context_;

  DISALLOW_COPY_AND_ASSIGN(CastMetricsServiceClient);
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_METRICS_CAST_METRICS_SERVICE_CLIENT_H_
