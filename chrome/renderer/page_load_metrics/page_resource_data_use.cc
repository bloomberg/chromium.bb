// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_load_metrics/page_resource_data_use.h"

#include "base/stl_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace page_load_metrics {

PageResourceDataUse::PageResourceDataUse()
    : resource_id_(-1),
      data_reduction_proxy_compression_ratio_estimate_(1.0),
      total_received_bytes_(0) {}

PageResourceDataUse::~PageResourceDataUse() = default;

void PageResourceDataUse::DidStartResponse(
    int resource_id,
    const network::ResourceResponseHead& response_head) {
  resource_id_ = resource_id;
  data_reduction_proxy_compression_ratio_estimate_ =
      data_reduction_proxy::EstimateCompressionRatioFromHeaders(&response_head);
  total_received_bytes_ = 0;
}

void PageResourceDataUse::DidReceiveTransferSizeUpdate(
    int received_data_length,
    mojom::PageLoadDataUse* delta_data_use) {
  delta_data_use->received_data_length += received_data_length;
  delta_data_use->data_reduction_proxy_bytes_saved +=
      received_data_length *
      (data_reduction_proxy_compression_ratio_estimate_ - 1.0);
  total_received_bytes_ += received_data_length;
}

bool PageResourceDataUse::DidCompleteResponse(
    const network::URLLoaderCompletionStatus& status,
    mojom::PageLoadDataUse* delta_data_use) {
  // Report the difference in received bytes.
  int64_t delta_bytes = status.encoded_data_length - total_received_bytes_;
  if (delta_bytes > 0) {
    delta_data_use->received_data_length += delta_bytes;
    delta_data_use->data_reduction_proxy_bytes_saved +=
        delta_bytes * (data_reduction_proxy_compression_ratio_estimate_ - 1.0);
    return true;
  }
  return false;
}

}  // namespace page_load_metrics
