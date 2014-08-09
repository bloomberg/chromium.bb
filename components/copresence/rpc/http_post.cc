// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/http_post.h"

// TODO(ckehoe): Support third-party protobufs too.
#include <google/protobuf/message_lite.h>

#include "base/bind.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace copresence {

// TODO(ckehoe): Come up with a better fix for Chromium.
#ifdef GOOGLE_CHROME_BUILD
namespace {

const char kApiKeyField[] = "key";

}  // namespace
#endif

const char HttpPost::kTracingTokenField[] = "trace";

HttpPost::HttpPost(net::URLRequestContextGetter* url_context_getter,
                   const std::string& server_host,
                   const std::string& rpc_name,
                   const std::string& tracing_token,
                   const google::protobuf::MessageLite& request_proto) {
  // Create the base URL to call.
  GURL url(server_host + "/" + rpc_name);

  // Add the Chrome API key.
  // TODO(ckehoe): Replace this with an app-specific key once the
  //               server API supports it. Also fix the Chromium case.
#ifdef GOOGLE_CHROME_BUILD
  DCHECK(google_apis::HasKeysConfigured());
  url = net::AppendQueryParameter(url, kApiKeyField, google_apis::GetAPIKey());
#endif

  // Add the tracing token, if specified.
  if (!tracing_token.empty()) {
    url = net::AppendQueryParameter(
        url, kTracingTokenField, "token:" + tracing_token);
  }

  // Serialize the proto for transmission.
  std::string request_data;
  bool serialize_success = request_proto.SerializeToString(&request_data);
  DCHECK(serialize_success);

  // Configure and send the request.
  url_fetcher_.reset(net::URLFetcher::Create(
      kUrlFetcherId, url, net::URLFetcher::POST, this));
  url_fetcher_->SetRequestContext(url_context_getter);
  url_fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE |
                             net::LOAD_DISABLE_CACHE |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->SetUploadData("application/x-protobuf", request_data);
}

HttpPost::~HttpPost() {}

void HttpPost::Start(const ResponseCallback& response_callback) {
  response_callback_ = response_callback;
  url_fetcher_->Start();
}

void HttpPost::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(url_fetcher_.get(), source);

  // Gather response info.
  std::string response;
  source->GetResponseAsString(&response);
  int response_code = source->GetResponseCode();

  // Log any errors.
  if (response_code < 0) {
    LOG(WARNING) << "Couldn't contact the Copresence server at "
                 << source->GetURL();
  } else if (response_code != net::HTTP_OK) {
    LOG(WARNING) << "Copresence request got HTTP response code "
                 << response_code << ". Response:\n" << response;
  }

  // Return the response.
  response_callback_.Run(response_code, response);
}

}  // namespace copresence
