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
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

using contextual_suggestions::ContextualSuggestionsMetricsReporter;
using contextual_suggestions::Cluster;
using contextual_suggestions::FetchClustersCallback;

ContextualContentSuggestionsService::ContextualContentSuggestionsService(
    std::unique_ptr<ContextualSuggestionsFetcher>
        contextual_suggestions_fetcher,
    std::unique_ptr<CachedImageFetcher> image_fetcher,
    std::unique_ptr<RemoteSuggestionsDatabase> contextual_suggestions_database,
    std::unique_ptr<ContextualSuggestionsMetricsReporter> metrics_reporter)
    : contextual_suggestions_database_(
          std::move(contextual_suggestions_database)),
      contextual_suggestions_fetcher_(
          std::move(contextual_suggestions_fetcher)),
      image_fetcher_(std::move(image_fetcher)),
      metrics_reporter_(std::move(metrics_reporter)),
      last_ukm_source_id_(ukm::kInvalidSourceId) {}

ContextualContentSuggestionsService::~ContextualContentSuggestionsService() =
    default;

// TODO(pnoland): remove this
void ContextualContentSuggestionsService::FetchContextualSuggestions(
    const GURL& url,
    FetchContextualSuggestionsCallback callback) {}

void ContextualContentSuggestionsService::FetchContextualSuggestionClusters(
    const GURL& url,
    FetchClustersCallback callback) {
  contextual_suggestions_fetcher_->FetchContextualSuggestionsClusters(
      url, std::move(callback));
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

void ContextualContentSuggestionsService::ReportEvent(
    ukm::SourceId ukm_source_id,
    contextual_suggestions::ContextualSuggestionsEvent event) {
  DCHECK(ukm_source_id != ukm::kInvalidSourceId);

  // Flush the previous page (if any) and setup the new page.
  if (ukm_source_id != last_ukm_source_id_) {
    if (last_ukm_source_id_ != ukm::kInvalidSourceId)
      metrics_reporter_->Flush();
    last_ukm_source_id_ = ukm_source_id;
    metrics_reporter_->SetupForPage(ukm_source_id);
  }

  metrics_reporter_->RecordEvent(event);
}

void ContextualContentSuggestionsService::Shutdown() {
  if (last_ukm_source_id_ != ukm::kInvalidSourceId)
    metrics_reporter_->Flush();
  last_ukm_source_id_ = ukm::kInvalidSourceId;
}

}  // namespace ntp_snippets
