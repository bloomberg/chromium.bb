// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/get_operation_request.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/offline_pages/core/prefetch/prefetch_proto_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_request_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {
const char kGetOperationURLPath[] = "v1/operations/";
}  // namespace

GetOperationRequest::GetOperationRequest(
    const std::string& name,
    net::URLRequestContextGetter* request_context_getter,
    const PrefetchRequestFinishedCallback& callback)
    : callback_(callback) {
  std::string path(kGetOperationURLPath);
  path += name;
  fetcher_ = PrefetchRequestFetcher::CreateForGet(
      path, request_context_getter,
      base::Bind(&GetOperationRequest::OnCompleted,
                 // Fetcher is owned by this instance.
                 base::Unretained(this)));
}

GetOperationRequest::~GetOperationRequest() {}

void GetOperationRequest::OnCompleted(PrefetchRequestStatus status,
                                      const std::string& data) {
  if (status != PrefetchRequestStatus::SUCCESS) {
    callback_.Run(status, std::vector<RenderPageInfo>());
    return;
  }

  std::vector<RenderPageInfo> pages;
  if (!ParseOperationResponse(data, &pages)) {
    callback_.Run(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
                  std::vector<RenderPageInfo>());
    return;
  }

  callback_.Run(PrefetchRequestStatus::SUCCESS, pages);
}

}  // offline_pages
