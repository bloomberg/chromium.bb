// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/contextual_json_request.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/utypes.h"

using net::URLFetcher;
using net::URLRequestContextGetter;
using net::HttpRequestHeaders;
using net::URLRequestStatus;

namespace ntp_snippets {

namespace internal {

namespace {

const int k5xxRetries = 2;

}  // namespace

ContextualJsonRequest::ContextualJsonRequest(const ParseJSONCallback& callback)
    : parse_json_callback_(callback), weak_ptr_factory_(this) {}

ContextualJsonRequest::~ContextualJsonRequest() {
  DLOG_IF(ERROR, !request_completed_callback_.is_null())
      << "The CompletionCallback was never called!";
}

void ContextualJsonRequest::Start(CompletedCallback callback) {
  request_completed_callback_ = std::move(callback);
  url_fetcher_->Start();
}

std::string ContextualJsonRequest::GetResponseString() const {
  std::string response;
  url_fetcher_->GetResponseAsString(&response);
  return response;
}

// URLFetcherDelegate overrides
void ContextualJsonRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(url_fetcher_.get(), source);
  const URLRequestStatus& status = url_fetcher_->GetStatus();
  int response = url_fetcher_->GetResponseCode();
  // TODO(gaschler): Add UMA metrics for response status code

  if (!status.is_success()) {
    std::move(request_completed_callback_)
        .Run(/*result=*/nullptr, FetchResult::URL_REQUEST_STATUS_ERROR,
             /*error_details=*/base::StringPrintf(" %d", status.error()));
  } else if (response != net::HTTP_OK) {
    // TODO(jkrcal): https://crbug.com/609084
    // We need to deal with the edge case again where the auth
    // token expires just before we send the request (in which case we need to
    // fetch a new auth token). We should extract that into a common class
    // instead of adding it to every single class that uses auth tokens.
    std::move(request_completed_callback_)
        .Run(/*result=*/nullptr, FetchResult::HTTP_ERROR,
             /*error_details=*/base::StringPrintf(" %d", response));
  } else {
    ParseJsonResponse();
  }
}

void ContextualJsonRequest::ParseJsonResponse() {
  std::string json_string;
  bool stores_result_to_string =
      url_fetcher_->GetResponseAsString(&json_string);
  DCHECK(stores_result_to_string);

  parse_json_callback_.Run(json_string,
                           base::Bind(&ContextualJsonRequest::OnJsonParsed,
                                      weak_ptr_factory_.GetWeakPtr()),
                           base::Bind(&ContextualJsonRequest::OnJsonError,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void ContextualJsonRequest::OnJsonParsed(std::unique_ptr<base::Value> result) {
  std::move(request_completed_callback_)
      .Run(std::move(result), FetchResult::SUCCESS,
           /*error_details=*/std::string());
}

void ContextualJsonRequest::OnJsonError(const std::string& error) {
  std::string json_string;
  url_fetcher_->GetResponseAsString(&json_string);
  LOG(WARNING) << "Received invalid JSON (" << error << "): " << json_string;
  std::move(request_completed_callback_)
      .Run(/*result=*/nullptr, FetchResult::JSON_PARSE_ERROR,
           /*error_details=*/base::StringPrintf(" (error %s)", error.c_str()));
}

ContextualJsonRequest::Builder::Builder() = default;
ContextualJsonRequest::Builder::Builder(ContextualJsonRequest::Builder&&) =
    default;
ContextualJsonRequest::Builder::~Builder() = default;

std::unique_ptr<ContextualJsonRequest> ContextualJsonRequest::Builder::Build()
    const {
  DCHECK(url_request_context_getter_);
  auto request = base::MakeUnique<ContextualJsonRequest>(parse_json_callback_);
  std::string body = BuildBody();
  std::string headers = BuildHeaders();
  request->url_fetcher_ = BuildURLFetcher(request.get(), headers, body);

  // Log the request for debugging network issues.
  VLOG(1) << "Sending a NTP snippets request to " << url_ << ":\n"
          << headers << "\n"
          << body;

  return request;
}

ContextualJsonRequest::Builder&
ContextualJsonRequest::Builder::SetAuthentication(
    const std::string& account_id,
    const std::string& auth_header) {
  auth_header_ = auth_header;
  return *this;
}

ContextualJsonRequest::Builder&
ContextualJsonRequest::Builder::SetParseJsonCallback(
    ParseJSONCallback callback) {
  parse_json_callback_ = callback;
  return *this;
}

ContextualJsonRequest::Builder& ContextualJsonRequest::Builder::SetUrl(
    const GURL& url) {
  url_ = url;
  return *this;
}

ContextualJsonRequest::Builder&
ContextualJsonRequest::Builder::SetUrlRequestContextGetter(
    const scoped_refptr<net::URLRequestContextGetter>& context_getter) {
  url_request_context_getter_ = context_getter;
  return *this;
}

ContextualJsonRequest::Builder& ContextualJsonRequest::Builder::SetContentUrl(
    const GURL& url) {
  content_url_ = url;
  return *this;
}

std::string ContextualJsonRequest::Builder::BuildHeaders() const {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Content-Type", "application/json; charset=UTF-8");
  if (!auth_header_.empty()) {
    headers.SetHeader("Authorization", auth_header_);
  }
  // Add X-Client-Data header with experiment IDs from field trials.
  // Note: It's OK to pass |is_signed_in| false if it's unknown, as it does
  // not affect transmission of experiments coming from the variations server.
  bool is_signed_in = false;
  variations::AppendVariationHeaders(url_,
                                     false,  // incognito
                                     false,  // uma_enabled
                                     is_signed_in, &headers);
  return headers.ToString();
}

std::string ContextualJsonRequest::Builder::BuildBody() const {
  auto request = base::MakeUnique<base::DictionaryValue>();

  request->SetString("url", content_url_.spec());
  auto categories = base::MakeUnique<base::ListValue>();
  categories->AppendString("RELATED_ARTICLES");
  categories->AppendString("PUBLIC_DEBATE");
  request->Set("categories", std::move(categories));

  std::string request_json;
  bool success = base::JSONWriter::WriteWithOptions(
      *request, base::JSONWriter::OPTIONS_PRETTY_PRINT, &request_json);
  DCHECK(success);
  return request_json;
}

std::unique_ptr<net::URLFetcher>
ContextualJsonRequest::Builder::BuildURLFetcher(
    net::URLFetcherDelegate* delegate,
    const std::string& headers,
    const std::string& body) const {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_contextual_suggestions_fetch",
                                          R"(
        semantics {
          sender: "New Tab Page Contextual Suggestions Fetch"
          description:
            "Chromium can show contextual suggestions that are related to the "
            "currently visited page on the New Tab page. "
          trigger:
            "Triggered when Home sheet is pulled up."
          data:
            "Only for a white-listed signed-in test user, the URL of the "
            "current tab."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled by the flag "
            "contextual-suggestions-carousel."
          chrome_policy {
            NTPContentSuggestionsEnabled {
              NTPContentSuggestionsEnabled: False
            }
          }
        })");
  std::unique_ptr<net::URLFetcher> url_fetcher = net::URLFetcher::Create(
      url_, net::URLFetcher::POST, delegate, traffic_annotation);
  url_fetcher->SetRequestContext(url_request_context_getter_.get());
  url_fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                            net::LOAD_DO_NOT_SAVE_COOKIES);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher.get(),
      data_use_measurement::DataUseUserData::NTP_SNIPPETS_SUGGESTIONS);

  url_fetcher->SetExtraRequestHeaders(headers);
  url_fetcher->SetUploadData("application/json", body);

  // Fetchers are sometimes cancelled because a network change was detected.
  url_fetcher->SetAutomaticallyRetryOnNetworkChanges(3);
  url_fetcher->SetMaxRetriesOn5xx(k5xxRetries);
  return url_fetcher;
}

}  // namespace internal

}  // namespace ntp_snippets
