// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestions_provider.h"

namespace gfx {
class Image;
}

namespace ntp_snippets {

class NTPSnippetsService;

// Retrieves suggestions from a number of ContentSuggestionsProviders and serves
// them grouped into categories. There can be at most one provider per category.
class ContentSuggestionsService : public KeyedService,
                                  public ContentSuggestionsProvider::Observer {
 public:
  using ImageFetchedCallback =
      base::Callback<void(const std::string& suggestion_id, const gfx::Image&)>;

  class Observer {
   public:
    // Fired every time the service receives a new set of data, replacing any
    // previously available data (though in most cases there will be an overlap
    // and only a few changes within the data). The new data is then available
    // through the getters of the service.
    virtual void OnNewSuggestions() = 0;

    // Fired when the status of a suggestions category changed. When the status
    // changes to an unavailable status, the suggestions of the respective
    // category have been invalidated, which means that they must no longer be
    // displayed to the user. The UI must immediately clear any suggestions of
    // that category.
    virtual void OnCategoryStatusChanged(Category category,
                                         CategoryStatus new_status) = 0;

    // Sent when the service is shutting down. After the service has shut down,
    // it will not provide any data anymore, though calling the getters is still
    // safe.
    virtual void ContentSuggestionsServiceShutdown() = 0;

   protected:
    virtual ~Observer() {}
  };

  enum State {
    ENABLED,
    DISABLED,
  };

  ContentSuggestionsService(State state);
  ~ContentSuggestionsService() override;

  // Inherited from KeyedService.
  void Shutdown() override;

  State state() { return state_; }

  // Gets all categories for which a provider is registered. The categories
  // may or may not be available, see |GetCategoryStatus()|.
  const std::vector<Category>& GetCategories() const { return categories_; }

  // Gets the status of a category.
  CategoryStatus GetCategoryStatus(Category category) const;

  // Gets the available suggestions for a category. The result is empty if the
  // category is available and empty, but also if the category is unavailable
  // for any reason, see |GetCategoryStatus()|.
  const std::vector<ContentSuggestion>& GetSuggestionsForCategory(
      Category category) const;

  // Fetches the image for the suggestion with the given |suggestion_id| and
  // runs the |callback|. If that suggestion doesn't exist or the fetch fails,
  // the callback gets an empty image.
  void FetchSuggestionImage(const std::string& suggestion_id,
                            const ImageFetchedCallback& callback);

  // Dismisses the suggestion with the given |suggestion_id|, if it exists.
  // This will not trigger an update through the observers.
  void DismissSuggestion(const std::string& suggestion_id);

  // Observer accessors.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Registers a new ContentSuggestionsProvider. It must be ensured that at most
  // one provider is registered for every category and that this method is
  // called only once per provider.
  void RegisterProvider(std::unique_ptr<ContentSuggestionsProvider> provider);

  // Only for debugging use through the internals page.
  // Removes all suggestions from all caches or internal stores in all
  // providers. It does, however, not remove any suggestions from the provider's
  // sources, so if their configuration hasn't changed, they should return the
  // same results when they fetch the next time. In particular, calling this
  // method will not mark any suggestions as dismissed.
  void ClearCachedSuggestionsForDebugging();

  // Only for debugging use through the internals page. Some providers
  // internally store a list of dismissed suggestions to prevent them from
  // reappearing. This function clears all such lists in all providers, making
  // dismissed suggestions reappear (only for certain providers).
  void ClearDismissedSuggestionsForDebugging();

  CategoryFactory* category_factory() { return &category_factory_; }

  // The reference to the NTPSnippetsService provider should only be set by the
  // factory and only be used for scheduling, periodic fetching and debugging.
  NTPSnippetsService* ntp_snippets_service() { return ntp_snippets_service_; }
  void set_ntp_snippets_service(NTPSnippetsService* ntp_snippets_service) {
    ntp_snippets_service_ = ntp_snippets_service;
  }

 private:
  friend class ContentSuggestionsServiceTest;

  // This is just an arbitrary ordering by ID, used by the maps in this class,
  // because the ordering needs to be constant for maps.
  struct CompareCategoriesByID {
    bool operator()(const Category& left, const Category& right) const;
  };

  // Implementation of ContentSuggestionsProvider::Observer.
  void OnNewSuggestions(ContentSuggestionsProvider* provider,
                        Category category,
                        std::vector<ContentSuggestion> suggestions) override;
  void OnCategoryStatusChanged(ContentSuggestionsProvider* provider,
                               Category category,
                               CategoryStatus new_status) override;

  // Registers the given |provider| for the given |category|, unless it is
  // already registered. Returns true if the category was newly registered or
  // false if it was present before.
  bool RegisterCategoryIfRequired(ContentSuggestionsProvider* provider,
                                  Category category);

  // Fires the OnCategoryStatusChanged event for the given |category|.
  void NotifyCategoryStatusChanged(Category category);

  // Whether the content suggestions feature is enabled.
  State state_;

  // Provides new and existing categories and an order for them.
  CategoryFactory category_factory_;

  // All registered providers, owned by the service.
  std::vector<std::unique_ptr<ContentSuggestionsProvider>> providers_;

  // All registered categories and their providers. A provider may be contained
  // multiple times, if it provides multiple categories. The keys of this map
  // are exactly the entries of |categories_| and the values are a subset of
  // |providers_|.
  std::map<Category, ContentSuggestionsProvider*, CompareCategoriesByID>
      providers_by_category_;

  // All current suggestion categories, in an order determined by the
  // |category_factory_|. This vector contains exactly the same categories as
  // |providers_by_category_|.
  std::vector<Category> categories_;

  // All current suggestions grouped by category. This contains an entry for
  // every category in |categories_| whose status is an available status. It may
  // contain an empty vector if the category is available but empty (or still
  // loading).
  std::map<Category, std::vector<ContentSuggestion>, CompareCategoriesByID>
      suggestions_by_category_;

  // Map used to determine the category of a suggestion (of which only the ID
  // is available). This also determines the provider that delivered the
  // suggestion.
  std::map<std::string, Category> id_category_map_;

  base::ObserverList<Observer> observers_;

  const std::vector<ContentSuggestion> no_suggestions_;

  // Keep a direct reference to this special provider to redirect scheduling,
  // background fetching and debugging calls to it. If the NTPSnippetsService is
  // loaded, it is also present in |providers_|, otherwise this is a nullptr.
  NTPSnippetsService* ntp_snippets_service_;

  DISALLOW_COPY_AND_ASSIGN(ContentSuggestionsService);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SERVICE_H_
