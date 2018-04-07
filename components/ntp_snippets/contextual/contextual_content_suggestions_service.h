// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_CONTENT_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_CONTENT_SUGGESTIONS_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_snippets/callbacks.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_fetcher.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_metrics_reporter.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ntp_snippets {

class CachedImageFetcher;
class RemoteSuggestionsDatabase;

// Retrieves contextual suggestions for a given URL and fetches images
// for contextual suggestion, using caching.
class ContextualContentSuggestionsService : public KeyedService {
 public:
  // A structure representing a suggestion cluster.
  struct Cluster {
   public:
    Cluster();
    explicit Cluster(const std::string& title);
    Cluster(Cluster&& other);
    ~Cluster();

    std::string title;
    ContextualSuggestion::PtrVector suggestions;

   private:
    DISALLOW_COPY_AND_ASSIGN(Cluster);
  };

  ContextualContentSuggestionsService(
      std::unique_ptr<ContextualSuggestionsFetcher>
          contextual_suggestions_fetcher,
      std::unique_ptr<CachedImageFetcher> image_fetcher,
      std::unique_ptr<RemoteSuggestionsDatabase>
          contextual_suggestions_database,
      std::unique_ptr<
          contextual_suggestions::ContextualSuggestionsMetricsReporter>
          metrics_reporter);
  ~ContextualContentSuggestionsService() override;

  using FetchContextualSuggestionsCallback =
      base::OnceCallback<void(Status status_code,
                              const GURL& url,
                              std::vector<ContentSuggestion> suggestions)>;

  using FetchContextualSuggestionClustersCallback =
      base::OnceCallback<void(std::string peek_text,
                              std::vector<Cluster> clusters)>;

  // Asynchronously fetches contextual suggestions for the given URL.
  virtual void FetchContextualSuggestions(
      const GURL& url,
      FetchContextualSuggestionsCallback callback);

  // Asynchronously fetches contextual suggestions for the given URL.
  virtual void FetchContextualSuggestionClusters(
      const GURL& url,
      FetchContextualSuggestionClustersCallback callback);

  // Fetches an image pointed to by |url| and internally caches it using
  // |suggestion_id|. Asynchronous if cache or network is queried.
  virtual void FetchContextualSuggestionImage(
      const ContentSuggestion::ID& suggestion_id,
      const GURL& url,
      ImageFetchedCallback callback);

  // Fetches an image for a given contextual suggestion ID.
  // Asynchronous if cache or network is queried.
  virtual void FetchContextualSuggestionImageLegacy(
      const ContentSuggestion::ID& suggestion_id,
      ImageFetchedCallback callback);

  // Used to report events using various metrics (e.g. UMA, UKM).
  virtual void ReportEvent(
      ukm::SourceId sourceId,
      contextual_suggestions::ContextualSuggestionsEvent event);

  // KeyedService overrides.
  void Shutdown() override;

 private:
  void DidFetchContextualSuggestions(
      const GURL& url,
      FetchContextualSuggestionsCallback callback,
      Status status,
      ContextualSuggestionsFetcher::OptionalSuggestions fetched_suggestions);

  // Temporary function to wire new bridge to the old prototype.
  void DidFetchContextualSuggestionsClusterWrapper(
      FetchContextualSuggestionClustersCallback callback,
      Status status,
      ContextualSuggestionsFetcher::OptionalSuggestions fetched_suggestions);

  // Cache for images of contextual suggestions, needed by CachedImageFetcher.
  std::unique_ptr<RemoteSuggestionsDatabase> contextual_suggestions_database_;

  // Performs actual network request to fetch contextual suggestions.
  std::unique_ptr<ContextualSuggestionsFetcher> contextual_suggestions_fetcher_;

  std::unique_ptr<CachedImageFetcher> image_fetcher_;

  std::unique_ptr<contextual_suggestions::ContextualSuggestionsMetricsReporter>
      metrics_reporter_;

  // The most recent SourceId in use by metrics_reporter_, or
  // ukm::kInvalidSourceId.
  ukm::SourceId last_ukm_source_id_;

  // Look up by ContentSuggestion::ID::id_within_category() aka std::string to
  // get image URL.
  std::map<std::string, GURL> image_url_by_id_;

  DISALLOW_COPY_AND_ASSIGN(ContextualContentSuggestionsService);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_CONTENT_SUGGESTIONS_SERVICE_H_
