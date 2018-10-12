// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_MEASUREMENT_H_
#define CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_MEASUREMENT_H_

#include <memory>

#include "base/macros.h"
#include "components/data_use_measurement/core/data_use_measurement.h"
#include "components/data_use_measurement/core/url_request_classifier.h"

namespace data_use_measurement {

class DataUseAscriber;

class ChromeDataUseMeasurement : public DataUseMeasurement {
 public:
  ChromeDataUseMeasurement(
      std::unique_ptr<URLRequestClassifier> url_request_classifier,
      DataUseAscriber* ascriber);

  void UpdateDataUseToMetricsService(int64_t total_bytes,
                                     bool is_cellular,
                                     bool is_metrics_service_usage) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeDataUseMeasurement);
};

}  // namespace data_use_measurement

#endif  // CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_MEASUREMENT_H_
