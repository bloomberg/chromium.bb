// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_METRICS_CAST_METRICS_SERVICE_CLIENT_H_
#define CHROMECAST_BROWSER_METRICS_CAST_METRICS_SERVICE_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/metrics/metrics_service_client.h"

class PrefService;

namespace base {
class MessageLoopProxy;
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
class ExternalMetrics;

class CastMetricsServiceClient : public ::metrics::MetricsServiceClient {
 public:
  virtual ~CastMetricsServiceClient();

  static CastMetricsServiceClient* Create(
      base::TaskRunner* io_task_runner,
      PrefService* pref_service,
      net::URLRequestContextGetter* request_context);

  // metrics::MetricsServiceClient implementation:
  virtual void SetMetricsClientId(const std::string& client_id) override;
  virtual bool IsOffTheRecordSessionActive() override;
  virtual int32_t GetProduct() override;
  virtual std::string GetApplicationLocale() override;
  virtual bool GetBrand(std::string* brand_code) override;
  virtual ::metrics::SystemProfileProto::Channel GetChannel() override;
  virtual std::string GetVersionString() override;
  virtual void OnLogUploadComplete() override;
  virtual void StartGatheringMetrics(
      const base::Closure& done_callback) override;
  virtual void CollectFinalMetrics(const base::Closure& done_callback) override;
  virtual scoped_ptr< ::metrics::MetricsLogUploader> CreateUploader(
      const std::string& server_url,
      const std::string& mime_type,
      const base::Callback<void(int)>& on_upload_complete) override;

  // Starts/stops the metrics service.
  void EnableMetricsService(bool enabled);

 private:
  CastMetricsServiceClient(
      base::TaskRunner* io_task_runner,
      PrefService* pref_service,
      net::URLRequestContextGetter* request_context);

  // Returns whether or not metrics reporting is enabled.
  bool IsReportingEnabled();

#if defined(OS_LINUX)
  scoped_ptr<ExternalMetrics> external_metrics_;
#endif  // defined(OS_LINUX)
  scoped_ptr< ::metrics::MetricsStateManager> metrics_state_manager_;
  scoped_ptr< ::metrics::MetricsService> metrics_service_;
  scoped_refptr<base::MessageLoopProxy> metrics_service_loop_;
  net::URLRequestContextGetter* request_context_;

  DISALLOW_COPY_AND_ASSIGN(CastMetricsServiceClient);
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_METRICS_CAST_METRICS_SERVICE_CLIENT_H_
