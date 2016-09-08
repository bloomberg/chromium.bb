// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/previews_data_savings.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/core/common/data_savings_recorder.h"

namespace previews {

PreviewsDataSavings::PreviewsDataSavings(
    data_reduction_proxy::DataSavingsRecorder* data_savings_recorder)
    : data_savings_recorder_(data_savings_recorder) {
  DCHECK(data_savings_recorder);
}

PreviewsDataSavings::~PreviewsDataSavings() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void PreviewsDataSavings::RecordDataSavings(
    const std::string& fully_qualified_domain_name,
    int64_t data_used,
    int64_t original_size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  data_savings_recorder_->UpdateDataSavings(fully_qualified_domain_name,
                                            data_used, original_size);
}

}  // namespace previews
