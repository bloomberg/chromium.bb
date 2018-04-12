// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_fetcher_impl.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ntp_snippets {

ContextualSuggestionsFetcherImpl::ContextualSuggestionsFetcherImpl(
    const scoped_refptr<network::SharedURLLoaderFactory>& loader_factory,
    const std::string& application_language_code)
    : loader_factory_(loader_factory),
      bcp_language_code_(application_language_code) {
  // Use loader_factory_ to suppress unused variable compiler warning.
  loader_factory_.get();
}

ContextualSuggestionsFetcherImpl::~ContextualSuggestionsFetcherImpl() = default;

void ContextualSuggestionsFetcherImpl::FetchContextualSuggestionsClusters(
    const GURL& url,
    FetchClustersCallback callback) {}

void ContextualSuggestionsFetcherImpl::FetchFinished(
    ContextualSuggestionsFetch* fetch,
    FetchClustersCallback callback,
    std::string peek_text,
    std::vector<Cluster> clusters) {}

}  // namespace ntp_snippets
