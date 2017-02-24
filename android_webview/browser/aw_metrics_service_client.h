// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_H_

#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_service_client.h"

class PrefService;

namespace base {
class FilePath;
}

namespace net {
class URLRequestContextGetter;
}

namespace android_webview {

class AwMetricsServiceClient : public metrics::MetricsServiceClient,
                               public metrics::EnabledStateProvider {
 public:
  static AwMetricsServiceClient* GetInstance();
  virtual void Initialize(PrefService* pref_service,
                          net::URLRequestContextGetter* request_context,
                          const base::FilePath guid_file_path) = 0;

 protected:
  AwMetricsServiceClient();
  ~AwMetricsServiceClient() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_H_
