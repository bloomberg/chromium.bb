// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_METRICS_SERVICE_CLIENT_IMPL_
#define ANDROID_WEBVIEW_NATIVE_AW_METRICS_SERVICE_CLIENT_IMPL_

#include "android_webview/browser/aw_metrics_service_client.h"

#include <jni.h>
#include <string>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/metrics/metrics_log_uploader.h"

namespace metrics {
class MetricsStateManager;
}

namespace android_webview {

// This singleton manages metrics for an app using any number of WebViews. It
// must always be used on the same thread. (Currently the UI thread is enforced,
// but it could be any thread.) This is to prevent enable/disable race
// conditions, and because MetricsService is single-threaded. Initialization is
// asynchronous; even after Initialize has returned, some methods may not be
// ready to use (see below).
class AwMetricsServiceClientImpl : public AwMetricsServiceClient {
  friend struct base::DefaultLazyInstanceTraits<AwMetricsServiceClientImpl>;

 public:
  void Initialize(PrefService* pref_service,
                  net::URLRequestContextGetter* request_context,
                  const base::FilePath guid_file_path) override;

  // metrics::EnabledStateProvider implementation
  bool IsConsentGiven() override;

  // The below functions must not be called until initialization has
  // asynchronously finished.

  void SetMetricsEnabled(bool enabled);

  // metrics::MetricsServiceClient implementation
  metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void InitializeSystemProfileMetrics(
      const base::Closure& done_callback) override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      const std::string& server_url,
      const std::string& mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const base::Callback<void(int)>& on_upload_complete) override;
  base::TimeDelta GetStandardUploadInterval() override;

 private:
  AwMetricsServiceClientImpl();
  ~AwMetricsServiceClientImpl() override;

  void InitializeWithGUID(std::string* guid);

  bool is_enabled_;
  PrefService* pref_service_;
  net::URLRequestContextGetter* request_context_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClientImpl);
};

bool RegisterAwMetricsServiceClient(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_METRICS_SERVICE_CLIENT_IMPL_
