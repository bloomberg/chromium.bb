// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/journey/journey_info_json_request.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/journey/proto/batch_get_switcher_journey_from_pageload_request.pb.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace journey {

namespace {

const int k5xxRetries = 2;

std::string GetSerializedJourneyRequest(
    const std::vector<int64_t>& timestamps) {
  BatchGetSwitcherJourneyFromPageloadRequest request;

  for (const auto timestamp : timestamps) {
    request.add_page_timestamp_usec(timestamp);
  }

  return request.SerializeAsString();
}

}  // namespace

JourneyInfoJsonRequest::JourneyInfoJsonRequest(
    const ParseJSONCallback& callback)
    : parse_json_callback_(callback), weak_ptr_factory_(this) {}

JourneyInfoJsonRequest::~JourneyInfoJsonRequest() {}

void JourneyInfoJsonRequest::Start(
    CompletedCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory>& loader_factory) {
  completed_callback_ = std::move(callback);
  last_response_string_.clear();

  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory.get(),
      base::BindOnce(&JourneyInfoJsonRequest::OnSimpleURLLoaderComplete,
                     base::Unretained(this)));
}

const std::string& JourneyInfoJsonRequest::GetResponseString() const {
  return last_response_string_;
}

void JourneyInfoJsonRequest::OnSimpleURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(simple_url_loader_);

  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }

  int net_error = simple_url_loader_->NetError();
  simple_url_loader_.reset();

  if (net_error != net::OK) {
    std::move(completed_callback_)
        .Run(nullptr, base::StringPrintf("Network error code: %d", net_error));
  } else if (response_code / 100 != 2) {
    std::move(completed_callback_)
        .Run(nullptr,
             base::StringPrintf("Http response error code: %d", response_code));
  } else {
    last_response_string_ = std::move(*response_body);
    parse_json_callback_.Run(
        last_response_string_,
        base::BindRepeating(&JourneyInfoJsonRequest::OnJsonParsed,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&JourneyInfoJsonRequest::OnJsonError,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void JourneyInfoJsonRequest::OnJsonParsed(std::unique_ptr<base::Value> result) {
  std::move(completed_callback_).Run(std::move(result), std::string());
}

void JourneyInfoJsonRequest::OnJsonError(const std::string& error) {
  DLOG(WARNING) << "Received invalid JSON (" << error
                << "): " << last_response_string_;
  std::move(completed_callback_).Run(nullptr, error);
}

JourneyInfoJsonRequest::Builder::Builder()
    : url_(GURL(
          "https://chrome-memex-dev.appspot.com/api/journey_from_pageload")) {}

JourneyInfoJsonRequest::Builder::~Builder() = default;

std::unique_ptr<JourneyInfoJsonRequest> JourneyInfoJsonRequest::Builder::Build()
    const {
  auto request = std::make_unique<JourneyInfoJsonRequest>(parse_json_callback_);
  request->simple_url_loader_ = BuildSimpleURLLoader();

  return request;
}

JourneyInfoJsonRequest::Builder&
JourneyInfoJsonRequest::Builder::SetAuthentication(
    const std::string& auth_header) {
  DVLOG(0) << "Authorization header " << auth_header;
  auth_header_ = auth_header;
  return *this;
}

JourneyInfoJsonRequest::Builder& JourneyInfoJsonRequest::Builder::SetTimestamps(
    const std::vector<int64_t>& timestamps) {
  body_ = GetSerializedJourneyRequest(timestamps);
  return *this;
}

JourneyInfoJsonRequest::Builder&
JourneyInfoJsonRequest::Builder::SetParseJsonCallback(
    ParseJSONCallback callback) {
  parse_json_callback_ = std::move(callback);
  return *this;
}

net::HttpRequestHeaders
JourneyInfoJsonRequest::Builder::BuildSimpleURLLoaderHeaders() const {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Content-Type", "application/json; charset=UTF-8");
  if (!auth_header_.empty()) {
    headers.SetHeader("Authorization", auth_header_);
  }
  variations::AppendVariationHeaders(url_, variations::InIncognito::kNo,
                                     variations::SignedIn::kNo, &headers);
  return headers;
}

std::unique_ptr<network::SimpleURLLoader>
JourneyInfoJsonRequest::Builder::BuildSimpleURLLoader() const {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url_);
  resource_request->load_flags =
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES;
  resource_request->headers = BuildSimpleURLLoaderHeaders();
  resource_request->method = "POST";

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), NO_TRAFFIC_ANNOTATION_YET);
  simple_loader->SetAllowHttpErrorResults(true);
  simple_loader->AttachStringForUpload(body_,
                                       "application/json; charset=UTF-8");
  simple_loader->SetRetryOptions(
      k5xxRetries, network::SimpleURLLoader::RetryMode::RETRY_ON_5XX);

  return simple_loader;
}

}  // namespace journey
