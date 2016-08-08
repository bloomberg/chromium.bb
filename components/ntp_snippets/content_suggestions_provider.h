// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"

namespace gfx {
class Image;
}

namespace ntp_snippets {

// Provides content suggestions from one particular source.
// A provider can provide suggestions for multiple ContentSuggestionCategories,
// but for every category that it provides, it will be the only provider in the
// system which provides suggestions for that category.
// Providers are created by the ContentSuggestionsServiceFactory and owned and
// shut down by the ContentSuggestionsService.
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
    // the status of the respective category. If the given |category| is not
    // known yet, the calling |provider| will be registered as its provider.
    // IDs for the ContentSuggestions should be generated with
    // |MakeUniqueID(..)| below.
    virtual void OnNewSuggestions(
        ContentSuggestionsProvider* provider,
        Category category,
        std::vector<ContentSuggestion> suggestions) = 0;

    // Called when the status of a category changed.
    // If |new_status| is NOT_PROVIDED, the calling provider must be the one
    // that currently provides the |category|, and the category is unregistered
    // without clearing the UI. The |category| must also be removed from
    // |GetProvidedCategories()|.
    // If |new_status| is any other value, it must match the value that is
    // currently returned from the provider's |GetCategoryStatus(category)|. In
    // case the given |category| is not known yet, the calling |provider| will
    // be registered as its provider. Whenever the status changes to an
    // unavailable status, all suggestions in that category must immediately be
    // removed from all caches and from the UI, but the provider remains
    // registered.
    virtual void OnCategoryStatusChanged(ContentSuggestionsProvider* provider,
                                         Category category,
                                         CategoryStatus new_status) = 0;
  };

  virtual ~ContentSuggestionsProvider();

  // Returns the categories provided by this provider.
  // When the set of provided categories changes, the Observer is notified
  // through |OnNewSuggestions| or |OnCategoryStatusChanged| for added
  // categories, and through |OnCategoryStatusChanged| with parameter
  // NOT_PROVIDED for removed categories.
  virtual std::vector<Category> GetProvidedCategories() = 0;

  // Determines the status of the given |category|, see CategoryStatus.
  virtual CategoryStatus GetCategoryStatus(Category category) = 0;

  // Dismisses the suggestion with the given ID. A provider needs to ensure that
  // a once-dismissed suggestion is never delivered again (through the
  // Observer). The provider must not call Observer::OnSuggestionsChanged if the
  // removal of the dismissed suggestion is the only change.
  virtual void DismissSuggestion(const std::string& suggestion_id) = 0;

  // Fetches the image for the suggestion with the given ID and returns it
  // through the callback. This fetch may occur locally or from the internet.
  // If that suggestion doesn't exist, doesn't have an image or if the fetch
  // fails, the callback gets a null image.
  virtual void FetchSuggestionImage(const std::string& suggestion_id,
                                    const ImageFetchedCallback& callback) = 0;

  // Used only for debugging purposes. Clears all caches so that the next
  // fetch starts from scratch.
  virtual void ClearCachedSuggestionsForDebugging() = 0;

  // Used only for debugging purposes. Clears the cache of dismissed
  // suggestions, if present, so that no suggestions are suppressed. This does
  // not necessarily make previously dismissed suggestions reappear, as they may
  // have been permanently deleted, depending on the provider implementation.
  virtual void ClearDismissedSuggestionsForDebugging() = 0;

 protected:
  ContentSuggestionsProvider(Observer* observer,
                             CategoryFactory* category_factory);

  // Creates a unique ID. The given |within_category_id| must be unique among
  // all suggestion IDs from this provider for the given |category|. This method
  // combines it with the |category| to form an ID that is unique
  // application-wide, because this provider is the only one that provides
  // suggestions for that category.
  std::string MakeUniqueID(Category category,
                           const std::string& within_category_id);

  // Reverse functions for MakeUniqueID()
  Category GetCategoryFromUniqueID(const std::string& unique_id);
  std::string GetWithinCategoryIDFromUniqueID(const std::string& unique_id);

  Observer* observer() const { return observer_; }
  CategoryFactory* category_factory() const { return category_factory_; }

 private:
  Observer* observer_;
  CategoryFactory* category_factory_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_H_
