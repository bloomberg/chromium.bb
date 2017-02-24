// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/test_metrics_log_uploader.h"
#include "components/metrics/metrics_log_uploader.h"

namespace metrics {

TestMetricsLogUploader::TestMetricsLogUploader(
    const std::string& server_url,
    const std::string& mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const base::Callback<void(int)>& on_upload_complete)
    : MetricsLogUploader(server_url,
                         mime_type,
                         service_type,
                         on_upload_complete),
      is_uploading_(false) {}

TestMetricsLogUploader::~TestMetricsLogUploader() = default;

void TestMetricsLogUploader::CompleteUpload(int response_code) {
  DCHECK(is_uploading_);
  is_uploading_ = false;
  on_upload_complete_.Run(response_code);
}

void TestMetricsLogUploader::UploadLog(const std::string& compressed_log_data,
                                       const std::string& log_hash) {
  DCHECK(!is_uploading_);
  is_uploading_ = true;
}

}  // namespace metrics
