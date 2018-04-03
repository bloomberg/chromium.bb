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
#include "components/ntp_snippets/remote/cached_image_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "ui/gfx/image/image.h"

namespace ntp_snippets {

ContextualContentSuggestionsService::Cluster::Cluster() = default;

ContextualContentSuggestionsService::Cluster::~Cluster() = default;

ContextualContentSuggestionsService::ContextualContentSuggestionsService(
    std::unique_ptr<ContextualSuggestionsFetcher>
        contextual_suggestions_fetcher,
    std::unique_ptr<CachedImageFetcher> image_fetcher,
    std::unique_ptr<RemoteSuggestionsDatabase> contextual_suggestions_database)
    : contextual_suggestions_database_(
          std::move(contextual_suggestions_database)),
      contextual_suggestions_fetcher_(
          std::move(contextual_suggestions_fetcher)),
      image_fetcher_(std::move(image_fetcher)) {}

ContextualContentSuggestionsService::~ContextualContentSuggestionsService() =
    default;

void ContextualContentSuggestionsService::FetchContextualSuggestions(
    const GURL& url,
    FetchContextualSuggestionsCallback callback) {
  contextual_suggestions_fetcher_->FetchContextualSuggestions(
      url,
      base::BindOnce(
          &ContextualContentSuggestionsService::DidFetchContextualSuggestions,
          base::Unretained(this), url, std::move(callback)));
}

void ContextualContentSuggestionsService::FetchContextualSuggestionClusters(
    const GURL& url,
    FetchContextualSuggestionClustersCallback callback) {
  // Fetch suggestions using the updated fetcher.
}

void ContextualContentSuggestionsService::FetchContextualSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    ImageFetchedCallback callback) {
  const std::string& id_within_category = suggestion_id.id_within_category();
  auto image_url_iterator = image_url_by_id_.find(id_within_category);
  if (image_url_iterator != image_url_by_id_.end()) {
    GURL image_url = image_url_iterator->second;
    image_fetcher_->FetchSuggestionImage(suggestion_id, image_url,
                                         ImageDataFetchedCallback(),
                                         std::move(callback));
  } else {
    DVLOG(1) << "FetchContextualSuggestionImage unknown image"
             << " id_within_category: " << id_within_category;
    std::move(callback).Run(gfx::Image());
  }
}

void ContextualContentSuggestionsService::ReportEvent(
    ukm::SourceId ukm_source_id,
    int event_id) {}

// TODO(gaschler): Cache contextual suggestions at run-time.
void ContextualContentSuggestionsService::DidFetchContextualSuggestions(
    const GURL& url,
    FetchContextualSuggestionsCallback callback,
    Status status,
    ContextualSuggestionsFetcher::OptionalSuggestions fetched_suggestions) {
  std::vector<ContentSuggestion> suggestions;
  if (fetched_suggestions.has_value()) {
    for (const std::unique_ptr<ContextualSuggestion>& suggestion :
         fetched_suggestions.value()) {
      suggestions.emplace_back(suggestion->ToContentSuggestion());
      ContentSuggestion::ID id = suggestions.back().id();
      GURL image_url = suggestion->salient_image_url();
      image_url_by_id_[id.id_within_category()] = image_url;
    }
  }
  std::move(callback).Run(status, url, std::move(suggestions));
}

}  // namespace ntp_snippets
