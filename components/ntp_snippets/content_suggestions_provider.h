// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_category.h"
#include "components/ntp_snippets/content_suggestions_category_status.h"

namespace gfx {
class Image;
}

namespace ntp_snippets {

// Provides content suggestions from one particular source.
// A provider can provide suggestions for multiple ContentSuggestionCategories,
// but for every category that it provides, it will be the only provider in the
// system which provides suggestions for that category.
// A provider can be a keyed service, in which case it should notify the
// ContentSuggestionsService through the observer before it shuts down.
class ContentSuggestionsProvider {
 public:
  using ImageFetchedCallback =
      base::Callback<void(const std::string& suggestion_id, const gfx::Image&)>;

  // The observer of a provider is notified when new data is available.
  class Observer {
   public:
    // Called when the available content changed.
    // If a provider provides suggestions for multiple categories, this callback
    // is called once per category. The |suggestions| parameter always contains
    // the full list of currently available suggestions for that category, i.e.,
    // an empty list will remove all suggestions from the given category. Note
    // that to clear them from the UI immediately, the provider needs to change
    // the status of the respective category.
    // IDs for the ContentSuggestions should be generated with
    // |MakeUniqueID(..)| below.
    virtual void OnNewSuggestions(
        ContentSuggestionsCategory changed_category,
        std::vector<ContentSuggestion> suggestions) = 0;

    // Called when the status of a category changed.
    // |new_status| must be the value that is currently returned from the
    // provider's |GetCategoryStatus(category)| below.
    // Whenever the status changes to an unavailable status, all suggestions in
    // that category must immediately be removed from all caches and from the
    // UI.
    virtual void OnCategoryStatusChanged(
        ContentSuggestionsCategory changed_category,
        ContentSuggestionsCategoryStatus new_status) = 0;

    // Called when the provider needs to shut down and will not deliver any
    // suggestions anymore.
    virtual void OnProviderShutdown(ContentSuggestionsProvider* provider) = 0;
  };

  // Sets an observer which is notified about changes to the available
  // suggestions, or removes it by passing a nullptr.
  virtual void SetObserver(Observer* observer) = 0;

  // Determines the status of the given |category|, see
  // ContentSuggestionsCategoryStatus.
  virtual ContentSuggestionsCategoryStatus GetCategoryStatus(
      ContentSuggestionsCategory category) = 0;

  // Discards the suggestion with the given ID. A provider needs to ensure that
  // a once-discarded suggestion is never delivered again (through the
  // Observer). The provider must not call Observer::OnSuggestionsChanged if the
  // removal of the discarded suggestion is the only change.
  virtual void DiscardSuggestion(const std::string& suggestion_id) = 0;

  // Fetches the image for the suggestion with the given ID and returns it
  // through the callback. This fetch may occur locally or from the internet.
  virtual void FetchSuggestionImage(const std::string& suggestion_id,
                                    const ImageFetchedCallback& callback) = 0;

  // Used only for debugging purposes. Clears all caches so that the next
  // fetch starts from scratch.
  virtual void ClearCachedSuggestionsForDebugging() = 0;

  // Used only for debugging purposes. Clears the cache of discarded
  // suggestions, if present, so that no suggestions are suppressed. This does
  // not necessarily make previously discarded suggestions reappear, as they may
  // have been permanently deleted, depending on the provider implementation.
  virtual void ClearDiscardedSuggestionsForDebugging() = 0;

  const std::vector<ContentSuggestionsCategory>& provided_categories() const {
    return provided_categories_;
  }

 protected:
  ContentSuggestionsProvider(
      const std::vector<ContentSuggestionsCategory>& provided_categories);
  virtual ~ContentSuggestionsProvider();

  // Creates a unique ID. The given |within_category_id| must be unique among
  // all suggestion IDs from this provider for the given |category|. This method
  // combines it with the |category| to form an ID that is unique
  // application-wide, because this provider is the only one that provides
  // suggestions for that category.
  static std::string MakeUniqueID(ContentSuggestionsCategory category,
                                  const std::string& within_category_id);

 private:
  const std::vector<ContentSuggestionsCategory> provided_categories_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_H_
