// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_RESPONSE_DATA_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_RESPONSE_DATA_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/blob_handle.h"

namespace content {

class BackgroundFetchRequestInfo;

using BackgroundFetchResponseCompleteCallback =
    base::Callback<void(std::vector<ServiceWorkerResponse>,
                        std::vector<std::unique_ptr<BlobHandle>>)>;

// Temporary holding class to aggregate the response data for requests
// associated with a given BackgroundFetch job.
class BackgroundFetchJobResponseData {
 public:
  BackgroundFetchJobResponseData(
      size_t num_requests,
      const BackgroundFetchResponseCompleteCallback& completion_callback);
  ~BackgroundFetchJobResponseData();

  void AddResponse(const BackgroundFetchRequestInfo& request_info,
                   std::unique_ptr<BlobHandle> response);
  bool IsComplete();

 private:
  std::vector<std::unique_ptr<BlobHandle>> blobs_;
  std::vector<ServiceWorkerResponse> responses_;
  size_t num_requests_ = 0;
  size_t completed_requests_ = 0;
  const std::string job_guid_;
  BackgroundFetchResponseCompleteCallback completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobResponseData);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_RESPONSE_DATA_H_
