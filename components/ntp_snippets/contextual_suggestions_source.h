// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_SUGGESTIONS_SOURCE_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_SUGGESTIONS_SOURCE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/ntp_snippets/callbacks.h"
#include "components/ntp_snippets/remote/contextual_suggestions_fetcher.h"

namespace ntp_snippets {

class CachedImageFetcher;
class RemoteSuggestionsDatabase;

// Retrieves contextual suggestions for a given URL and fetches images
// for contextual suggestion, using caching.
class ContextualSuggestionsSource {
 public:
  ContextualSuggestionsSource(std::unique_ptr<ContextualSuggestionsFetcher>
                                  contextual_suggestions_fetcher,
                              std::unique_ptr<CachedImageFetcher> image_fetcher,
                              std::unique_ptr<RemoteSuggestionsDatabase>
                                  contextual_suggestions_database);
  ~ContextualSuggestionsSource();

  using FetchContextualSuggestionsCallback =
      base::OnceCallback<void(Status status_code,
                              const GURL& url,
                              std::vector<ContentSuggestion> suggestions)>;

  // Asynchronously fetches contextual suggestions for the given URL.
  void FetchContextualSuggestions(const GURL& url,
                                  FetchContextualSuggestionsCallback callback);

  // Fetches an image for a given contextual suggestion ID.
  // Asynchronous if cache or network is queried.
  void FetchContextualSuggestionImage(
      const ContentSuggestion::ID& suggestion_id,
      const ImageFetchedCallback& callback);

 private:
  void DidFetchContextualSuggestions(
      const GURL& url,
      FetchContextualSuggestionsCallback callback,
      Status status,
      ContextualSuggestionsFetcher::OptionalSuggestions fetched_suggestions);

  // Cache for images of contextual suggestions, needed by CachedImageFetcher.
  std::unique_ptr<RemoteSuggestionsDatabase> contextual_suggestions_database_;

  // Performs actual network request to fetch contextual suggestions.
  std::unique_ptr<ContextualSuggestionsFetcher> contextual_suggestions_fetcher_;

  std::unique_ptr<CachedImageFetcher> image_fetcher_;

  // Look up by ContentSuggestion::ID::id_within_category() aka std::string to
  // get image URL.
  std::map<std::string, GURL> image_url_by_id_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSuggestionsSource);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_SUGGESTIONS_SOURCE_H_
