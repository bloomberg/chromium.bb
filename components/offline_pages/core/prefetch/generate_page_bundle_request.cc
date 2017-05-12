// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/offline_pages/core/prefetch/prefetch_request_fetcher.h"
#include "components/offline_pages/core/prefetch/prefetch_utils.h"
#include "components/offline_pages/core/prefetch/proto/offline_pages.pb.h"
#include "components/offline_pages/core/prefetch/proto/operation.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {
// TODO(jianli): Update when server is ready.
const GURL kOfflinePrefetchServiceUrl(
    "http://localhost:12345/v1:GeneratePageBundle");
}

GeneratePageBundleRequest::GeneratePageBundleRequest(
    const std::string& user_agent,
    const std::string& gcm_registration_id,
    int max_bundle_size_bytes,
    const std::vector<std::string>& page_urls,
    net::URLRequestContextGetter* request_context_getter,
    const FinishedCallback& callback)
    : callback_(callback) {
  proto::GeneratePageBundleRequest request;
  request.set_user_agent(user_agent);
  request.set_max_bundle_size_bytes(max_bundle_size_bytes);
  request.set_output_format(proto::FORMAT_MHTML);
  request.set_gcm_registration_id(gcm_registration_id);

  for (const auto& page_url : page_urls) {
    proto::PageParameters* page = request.add_pages();
    page->set_url(page_url);
    page->set_transformation(proto::NO_TRANSFORMATION);
  }

  std::string upload_data;
  request.SerializeToString(&upload_data);

  fetcher_.reset(new PrefetchRequestFetcher(
      kOfflinePrefetchServiceUrl, upload_data, request_context_getter,
      base::Bind(&GeneratePageBundleRequest::OnCompleted,
                 // Fetcher is owned by this instance.
                 base::Unretained(this))));
}

GeneratePageBundleRequest::~GeneratePageBundleRequest() {}

void GeneratePageBundleRequest::OnCompleted(PrefetchRequestStatus status,
                                            const std::string& data) {
  proto::Operation operation;
  if (!operation.ParseFromString(data)) {
    DVLOG(1) << "Failed to parse operation";
    NotifyParsingFailure();
    return;
  }

  if (operation.done())
    ParseDoneOperationResponse(operation);
  else
    ParsePendingOperationResponse(operation);
}

void GeneratePageBundleRequest::ParseDoneOperationResponse(
    const proto::Operation& operation) {
  switch (operation.result_case()) {
    case proto::Operation::kError:
      DVLOG(1) << "Error found in operation";
      NotifyParsingFailure();
      break;
    case proto::Operation::kResponse:
      ParseAnyData(operation.response());
      break;
    default:
      DVLOG(1) << "Result not set in operation";
      NotifyParsingFailure();
      break;
  }
}

void GeneratePageBundleRequest::ParsePendingOperationResponse(
    const proto::Operation& operation) {
  if (!operation.has_metadata()) {
    DVLOG(1) << "metadata not found in GeneratePageBundle response";
    NotifyParsingFailure();
    return;
  }

  ParseAnyData(operation.metadata());
}

void GeneratePageBundleRequest::ParseAnyData(const proto::Any& any_data) {
  std::vector<RenderPageInfo> pages;
  if (!ParsePageBundleInAnyData(any_data, &pages)) {
    NotifyParsingFailure();
    return;
  }

  callback_.Run(PrefetchRequestStatus::SUCCESS, pages);
}

void GeneratePageBundleRequest::NotifyParsingFailure() {
  callback_.Run(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
                std::vector<RenderPageInfo>());
}

}  // offline_pages
