// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_IMPL_H_

#include "android_webview/browser/aw_metrics_service_client.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/metrics/metrics_service_client.h"

class PrefService;

namespace base {
class FilePath;
}

namespace metrics {
struct ClientInfo;
class MetricsStateManager;
}

namespace net {
class URLRequestContextGetter;
}

namespace android_webview {

// This singleton manages metrics for an app using any number of WebViews.
// Metrics is turned on and off by the homonymous Java class. It should only be
// used on the main thread. In particular, Initialize, Finalize, and
// SetMetricsEnabled must be called from the same thread, in order to prevent
// enable/disable race conditions, and because MetricsService is
// single-threaded.
class AwMetricsServiceClient : public metrics::MetricsServiceClient {
  friend struct base::DefaultLazyInstanceTraits<AwMetricsServiceClient>;

 public:
  static AwMetricsServiceClient* GetInstance();

  void Initialize(PrefService* pref_service,
                  net::URLRequestContextGetter* request_context,
                  const base::FilePath guid_file_path);
  void Finalize();
  void SetMetricsEnabled(bool enabled);

  // metrics::MetricsServiceClient implementation
  metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  void OnRecordingDisabled() override;
  bool IsOffTheRecordSessionActive() override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void OnLogUploadComplete() override;
  void InitializeSystemProfileMetrics(
      const base::Closure& done_callback) override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  scoped_ptr<metrics::MetricsLogUploader> CreateUploader(
      const base::Callback<void(int)>& on_upload_complete) override;
  base::TimeDelta GetStandardUploadInterval() override;

 private:
  AwMetricsServiceClient();
  ~AwMetricsServiceClient() override;

  void InitializeWithGUID(std::string* guid);

  // Callback for metrics::MetricsStateManager::Create
  bool is_reporting_enabled();

  bool is_initialized_;
  bool is_enabled_;
  PrefService* pref_service_;
  net::URLRequestContextGetter* request_context_;
  scoped_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  scoped_ptr<metrics::MetricsService> metrics_service_;

  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_IMPL_H_
