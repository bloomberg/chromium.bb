// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_TEST_METRICS_SERVICE_CLIENT_H_
#define COMPONENTS_METRICS_TEST_METRICS_SERVICE_CLIENT_H_

#include "components/metrics/metrics_service_client.h"

namespace metrics {

// A simple concrete implementation of the MetricsServiceClient interface, for
// use in tests.
class TestMetricsServiceClient : public MetricsServiceClient {
 public:
  static const char kBrandForTesting[];

  TestMetricsServiceClient();
  virtual ~TestMetricsServiceClient();

  // MetricsServiceClient:
  virtual void SetClientID(const std::string& client_id) OVERRIDE;
  virtual bool IsOffTheRecordSessionActive() OVERRIDE;
  virtual std::string GetApplicationLocale() OVERRIDE;
  virtual bool GetBrand(std::string* brand_code) OVERRIDE;
  virtual SystemProfileProto::Channel GetChannel() OVERRIDE;
  virtual std::string GetVersionString() OVERRIDE;
  virtual void OnLogUploadComplete() OVERRIDE;

  const std::string& get_client_id() const { return client_id_; }

 private:
  std::string client_id_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_TEST_METRICS_SERVICE_CLIENT_H_
