// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_response_data.h"

namespace content {

BackgroundFetchJobResponseData::BackgroundFetchJobResponseData(
    size_t num_requests,
    const BackgroundFetchResponseCompleteCallback& completion_callback)
    : blobs_(num_requests),
      num_requests_(num_requests),
      completion_callback_(std::move(completion_callback)) {}

BackgroundFetchJobResponseData::~BackgroundFetchJobResponseData() {}

void BackgroundFetchJobResponseData::AddResponse(
    int request_num,
    std::unique_ptr<BlobHandle> response) {
  blobs_[request_num] = std::move(response);
  completed_requests_++;

  if (completed_requests_ == num_requests_) {
    std::move(completion_callback_).Run(std::move(blobs_));
  }
}

bool BackgroundFetchJobResponseData::IsComplete() {
  return completed_requests_ == num_requests_;
}

}  // namespace content
