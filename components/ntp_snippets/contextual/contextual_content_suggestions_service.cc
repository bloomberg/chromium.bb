// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_content_suggestions_service.h"

#include <iterator>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/ntp_snippets/contextual/cluster.h"
#include "components/ntp_snippets/remote/cached_image_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "contextual_content_suggestions_service_proxy.h"
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

using contextual_suggestions::Cluster;
using contextual_suggestions::ContextualSuggestionsMetricsReporterProvider;
using contextual_suggestions::FetchClustersCallback;

namespace {
bool IsEligibleURL(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS() && !url.HostIsIPAddress();
}

}  // namespace

ContextualContentSuggestionsService::ContextualContentSuggestionsService(
    std::unique_ptr<ContextualSuggestionsFetcher>
        contextual_suggestions_fetcher,
    std::unique_ptr<CachedImageFetcher> image_fetcher,
    std::unique_ptr<RemoteSuggestionsDatabase> contextual_suggestions_database,
    std::unique_ptr<ContextualSuggestionsMetricsReporterProvider>
        metrics_reporter_provider)
    : contextual_suggestions_database_(
          std::move(contextual_suggestions_database)),
      contextual_suggestions_fetcher_(
          std::move(contextual_suggestions_fetcher)),
      image_fetcher_(std::move(image_fetcher)),
      metrics_reporter_provider_(std::move(metrics_reporter_provider)) {}

ContextualContentSuggestionsService::~ContextualContentSuggestionsService() =
    default;

void ContextualContentSuggestionsService::FetchContextualSuggestionClusters(
    const GURL& url,
    FetchClustersCallback callback,
    ReportFetchMetricsCallback metrics_callback) {
  // TODO(pnoland): Also check that the url is safe.
  if (IsEligibleURL(url)) {
    contextual_suggestions_fetcher_->FetchContextualSuggestionsClusters(
        url, std::move(callback), std::move(metrics_callback));
  } else {
    std::move(callback).Run("", {});
  }
}

void ContextualContentSuggestionsService::FetchContextualSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const GURL& image_url,
    ImageFetchedCallback callback) {
  image_fetcher_->FetchSuggestionImage(suggestion_id, image_url,
                                       ImageDataFetchedCallback(),
                                       std::move(callback));
}

void ContextualContentSuggestionsService::FetchContextualSuggestionImageLegacy(
    const ContentSuggestion::ID& suggestion_id,
    ImageFetchedCallback callback) {
  const std::string& id_within_category = suggestion_id.id_within_category();
  auto image_url_iterator = image_url_by_id_.find(id_within_category);
  if (image_url_iterator == image_url_by_id_.end()) {
    DVLOG(1) << "FetchContextualSuggestionImage unknown image"
             << " id_within_category: " << id_within_category;
    std::move(callback).Run(gfx::Image());
    return;
  }

  GURL image_url = image_url_iterator->second;
  FetchContextualSuggestionImage(suggestion_id, image_url, std::move(callback));
}

std::unique_ptr<
    contextual_suggestions::ContextualContentSuggestionsServiceProxy>
ContextualContentSuggestionsService::CreateProxy() {
  return std::make_unique<
      contextual_suggestions::ContextualContentSuggestionsServiceProxy>(
      this, metrics_reporter_provider_->CreateMetricsReporter());
}

}  // namespace ntp_snippets
