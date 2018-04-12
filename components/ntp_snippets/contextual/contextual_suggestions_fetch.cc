// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_fetch.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/ntp_snippets/contextual/contextual_suggestion.h"
#include "components/ntp_snippets/contextual/proto/chrome_search_api_request_context.pb.h"
#include "components/ntp_snippets/contextual/proto/get_pivots_request.pb.h"
#include "components/ntp_snippets/contextual/proto/get_pivots_response.pb.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace contextual_suggestions {

namespace {

static constexpr char kFetchEndpoint[] =
    "https://www.google.com/httpservice/web/ExploreService/GetPivots/";
}

ContextualSuggestionsFetch::ContextualSuggestionsFetch(
    const GURL& url,
    const std::string& bcp_language_code)
    : url_(url), bcp_language_code_(bcp_language_code) {}

ContextualSuggestionsFetch::~ContextualSuggestionsFetch() = default;

// static
const std::string ContextualSuggestionsFetch::GetFetchEndpoint() {
  return kFetchEndpoint;
}

void ContextualSuggestionsFetch::Start(
    FetchClustersCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory>& loader_factory) {}

std::unique_ptr<network::SimpleURLLoader>
ContextualSuggestionsFetch::MakeURLLoader() const {
  return nullptr;
}

net::HttpRequestHeaders ContextualSuggestionsFetch::MakeHeaders() const {
  return net::HttpRequestHeaders();
}

void ContextualSuggestionsFetch::OnURLLoaderComplete(
    std::unique_ptr<std::string> result) {}

}  // namespace contextual_suggestions