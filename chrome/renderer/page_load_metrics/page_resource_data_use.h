// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_RESOURCE_DATA_USE_H_
#define CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_RESOURCE_DATA_USE_H_

#include "base/macros.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"

namespace network {
struct ResourceResponseHead;
struct URLLoaderCompletionStatus;
}  // namespace network

namespace page_load_metrics {

// PageResourceDataUse contains the data use information of one resource. Data
// use is updated when resource size updates are called.
class PageResourceDataUse {
 public:
  PageResourceDataUse();
  ~PageResourceDataUse();

  void DidStartResponse(int resource_id,
                        const network::ResourceResponseHead& response_head);

  // Updates any additional bytes of data use to |delta_data_use|.
  void DidReceiveTransferSizeUpdate(int received_data_length,
                                    mojom::PageLoadDataUse* delta_data_use);

  // Updates additional bytes to |delta_data_use| and returns whether it was
  // updated.
  bool DidCompleteResponse(const network::URLLoaderCompletionStatus& status,
                           mojom::PageLoadDataUse* delta_data_use);

  int resource_id() const { return resource_id_; }

 private:
  int resource_id_;

  // Compression ratio estimated from the response headers if data saver was
  // used.
  double data_reduction_proxy_compression_ratio_estimate_;

  uint64_t total_received_bytes_;

  DISALLOW_ASSIGN(PageResourceDataUse);
};

}  // namespace page_load_metrics

#endif  // CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_RESOURCE_DATA_USE_H_
