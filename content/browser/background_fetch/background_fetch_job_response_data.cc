// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_response_data.h"

#include "base/time/time.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "url/gurl.h"

namespace content {

BackgroundFetchJobResponseData::BackgroundFetchJobResponseData(
    size_t num_requests,
    const BackgroundFetchResponseCompleteCallback& completion_callback)
    : num_requests_(num_requests),
      completion_callback_(std::move(completion_callback)) {}

BackgroundFetchJobResponseData::~BackgroundFetchJobResponseData() {}

void BackgroundFetchJobResponseData::AddResponse(
    const BackgroundFetchRequestInfo& request_info,
    std::unique_ptr<BlobHandle> response) {
  auto urls = base::MakeUnique<std::vector<GURL>>(std::vector<GURL>());
  urls->push_back(request_info.url());

  // TODO(harkness): Fill in headers and status code/text.
  auto headers = base::MakeUnique<ServiceWorkerHeaderMap>();
  responses_.emplace_back(
      std::move(urls), 0 /* status code */, std::string("") /* status text */,
      blink::WebServiceWorkerResponseTypeBasic, std::move(headers),
      response->GetUUID(), request_info.received_bytes(), GURL(),
      blink::WebServiceWorkerResponseErrorUnknown,
      base::Time() /* response time */, false /* is_in_cache_storage */,
      std::string("") /* cache_storage_cache_name */,
      base::MakeUnique<ServiceWorkerHeaderList>());

  blobs_.push_back(std::move(response));
  completed_requests_++;

  if (completed_requests_ == num_requests_) {
    std::move(completion_callback_)
        .Run(std::move(responses_), std::move(blobs_));
  }
}

bool BackgroundFetchJobResponseData::IsComplete() {
  return completed_requests_ == num_requests_;
}

}  // namespace content
