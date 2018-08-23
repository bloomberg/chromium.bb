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
      total_received_bytes_(0),
      last_update_bytes_(0),
      is_complete_(false),
      is_canceled_(false) {}

PageResourceDataUse::~PageResourceDataUse() = default;

void PageResourceDataUse::DidStartResponse(
    int resource_id,
    const network::ResourceResponseHead& response_head) {
  resource_id_ = resource_id;
  data_reduction_proxy_compression_ratio_estimate_ =
      data_reduction_proxy::EstimateCompressionRatioFromHeaders(&response_head);
  total_received_bytes_ = 0;
  last_update_bytes_ = 0;
}

void PageResourceDataUse::DidReceiveTransferSizeUpdate(
    int received_data_length) {
  total_received_bytes_ += received_data_length;
}

bool PageResourceDataUse::DidCompleteResponse(
    const network::URLLoaderCompletionStatus& status) {
  // Report the difference in received bytes.
  is_complete_ = true;
  int64_t delta_bytes = status.encoded_data_length - total_received_bytes_;
  if (delta_bytes > 0) {
    total_received_bytes_ += delta_bytes;
    return true;
  }
  return false;
}

void PageResourceDataUse::DidCancelResponse() {
  is_canceled_ = true;
}

bool PageResourceDataUse::IsFinishedLoading() {
  return is_complete_ || is_canceled_;
}

int PageResourceDataUse::CalculateNewlyReceivedBytes() {
  int newly_received_bytes = total_received_bytes_ - last_update_bytes_;
  last_update_bytes_ = total_received_bytes_;
  DCHECK(newly_received_bytes >= 0);
  return newly_received_bytes;
}

mojom::ResourceDataUpdatePtr PageResourceDataUse::GetResourceDataUpdate() {
  mojom::ResourceDataUpdatePtr resource_data_update =
      mojom::ResourceDataUpdate::New();
  resource_data_update->request_id = resource_id();
  resource_data_update->received_data_length = total_received_bytes_;
  resource_data_update->delta_bytes = CalculateNewlyReceivedBytes();
  resource_data_update->is_complete = is_complete_;
  resource_data_update->data_reduction_proxy_compression_ratio_estimate =
      data_reduction_proxy_compression_ratio_estimate_;
  return resource_data_update;
}
}  // namespace page_load_metrics
