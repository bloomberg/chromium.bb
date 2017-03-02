// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_IMPL_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_IMPL_H_

#include <cstddef>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_status_service.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/ntp_snippets/remote/request_throttler.h"

class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageDecoder;
class ImageFetcher;
}  // namespace image_fetcher

namespace ntp_snippets {

class CategoryRanker;
class RemoteSuggestionsDatabase;
class RemoteSuggestionsScheduler;

// CachedImageFetcher takes care of fetching images from the network and caching
// them in the database.
// TODO(tschumann): Move into a separate library and inject the
// CachedImageFetcher into the RemoteSuggestionsProvider. This allows us to get
// rid of exposing this member for testing and lets us test the caching logic
// separately.
class CachedImageFetcher : public image_fetcher::ImageFetcherDelegate {
 public:
  // |pref_service| and |database| need to outlive the created image fetcher
  // instance.
  CachedImageFetcher(std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
                     std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
                     PrefService* pref_service,
                     RemoteSuggestionsDatabase* database);
  ~CachedImageFetcher() override;

  // Fetches the image for a suggestion. The fetcher will first issue a lookup
  // to the underlying cache with a fallback to the network.
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            const GURL& image_url,
                            const ImageFetchedCallback& callback);

 private:
  // image_fetcher::ImageFetcherDelegate implementation.
  void OnImageDataFetched(const std::string& id_within_category,
                          const std::string& image_data) override;

  void OnImageDecodingDone(const ImageFetchedCallback& callback,
                           const std::string& id_within_category,
                           const gfx::Image& image);
  void OnImageFetchedFromDatabase(
      const ImageFetchedCallback& callback,
      const ContentSuggestion::ID& suggestion_id,
      const GURL& image_url,
      // SnippetImageCallback requires by-value (not const ref).
      std::string data);
  void OnImageDecodedFromDatabase(const ImageFetchedCallback& callback,
                                  const ContentSuggestion::ID& suggestion_id,
                                  const GURL& url,
                                  const gfx::Image& image);
  void FetchImageFromNetwork(const ContentSuggestion::ID& suggestion_id,
                             const GURL& url,
                             const ImageFetchedCallback& callback);

  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;
  RemoteSuggestionsDatabase* database_;
  // Request throttler for limiting requests to thumbnail images.
  RequestThrottler thumbnail_requests_throttler_;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcher);
};

// Retrieves fresh content data (articles) from the server, stores them and
// provides them as content suggestions.
// This class is final because it does things in its constructor which make it
// unsafe to derive from it.
// TODO(treib): Introduce two-phase initialization and make the class not final?
class RemoteSuggestionsProviderImpl final : public RemoteSuggestionsProvider {
 public:
  // |application_language_code| should be a ISO 639-1 compliant string, e.g.
  // 'en' or 'en-US'. Note that this code should only specify the language, not
  // the locale, so 'en_US' (English language with US locale) and 'en-GB_US'
  // (British English person in the US) are not language codes.
  RemoteSuggestionsProviderImpl(
      Observer* observer,
      PrefService* pref_service,
      const std::string& application_language_code,
      CategoryRanker* category_ranker,
      std::unique_ptr<RemoteSuggestionsFetcher> suggestions_fetcher,
      std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
      std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
      std::unique_ptr<RemoteSuggestionsDatabase> database,
      std::unique_ptr<RemoteSuggestionsStatusService> status_service);

  ~RemoteSuggestionsProviderImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns whether the service is ready. While this is false, the list of
  // suggestions will be empty, and all modifications to it (fetch, dismiss,
  // etc) will be ignored.
  bool ready() const { return state_ == State::READY; }

  // Returns whether the service is successfully initialized. While this is
  // false, some calls may trigger DCHECKs.
  bool initialized() const { return ready() || state_ == State::DISABLED; }

  // Set the scheduler to be notified whenever the provider becomes active /
  // in-active and whenever history is deleted. The initial change is also
  // notified (switching from an initial undecided status). If the scheduler is
  // set after the first change, it is called back immediately.
  void SetRemoteSuggestionsScheduler(RemoteSuggestionsScheduler* scheduler);

  // RemoteSuggestionsProvider implementation.
  void RefetchInTheBackground(
      std::unique_ptr<FetchStatusCallback> callback) override;

  // TODO(fhorschig): Remove this getter when there is an interface for the
  // fetcher that allows better mocks.
  const RemoteSuggestionsFetcher* suggestions_fetcher_for_debugging()
      const override;

  // ContentSuggestionsProvider implementation.
  CategoryStatus GetCategoryStatus(Category category) override;
  CategoryInfo GetCategoryInfo(Category category) override;
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id) override;
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            const ImageFetchedCallback& callback) override;
  void Fetch(const Category& category,
             const std::set<std::string>& known_suggestion_ids,
             const FetchDoneCallback& callback) override;
  void ReloadSuggestions() override;
  void ClearHistory(
      base::Time begin,
      base::Time end,
      const base::Callback<bool(const GURL& url)>& filter) override;
  void ClearCachedSuggestions(Category category) override;
  void OnSignInStateChanged() override;
  void GetDismissedSuggestionsForDebugging(
      Category category,
      const DismissedSuggestionsCallback& callback) override;
  void ClearDismissedSuggestionsForDebugging(Category category) override;

  // Returns the maximum number of suggestions that will be shown at once.
  static int GetMaxSuggestionCountForTesting();

  // Available suggestions, only for unit tests.
  // TODO(treib): Get rid of this. Tests should use a fake observer instead.
  const RemoteSuggestion::PtrVector& GetSuggestionsForTesting(
      Category category) const {
    return category_contents_.find(category)->second.suggestions;
  }

  // Dismissed suggestions, only for unit tests.
  const RemoteSuggestion::PtrVector& GetDismissedSuggestionsForTesting(
      Category category) const {
    return category_contents_.find(category)->second.dismissed;
  }

  // Overrides internal clock for testing purposes.
  void SetClockForTesting(std::unique_ptr<base::Clock> clock) {
    clock_ = std::move(clock);
  }

  // TODO(tschumann): remove this method as soon as we inject the fetcher into
  // the constructor.
  CachedImageFetcher& GetImageFetcherForTesting() { return image_fetcher_; }

 private:
  friend class RemoteSuggestionsProviderImplTest;

  // TODO(jkrcal): Mock the database to trigger the error naturally (or remove
  // the error state and get rid of the test).
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           CallsSchedulerOnError);
  // TODO(jkrcal): Mock the status service and remove these friend declarations.
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           CallsSchedulerWhenDisabled);
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           DontNotifyIfNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           CallsSchedulerWhenSignedIn);
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           CallsSchedulerWhenSignedOut);

  // Possible state transitions:
  //       NOT_INITED --------+
  //       /       \          |
  //      v         v         |
  //   READY <--> DISABLED    |
  //       \       /          |
  //        v     v           |
  //     ERROR_OCCURRED <-----+
  // TODO(jkrcal): Do we need to keep the distinction between states DISABLED
  // and ERROR_OCCURED?
  enum class State {
    // The service has just been created. Can change to states:
    // - DISABLED: After the database is done loading,
    //             GetStateForDependenciesStatus can identify the next state to
    //             be DISABLED.
    // - READY: if GetStateForDependenciesStatus returns it, after the database
    //          is done loading.
    // - ERROR_OCCURRED: when an unrecoverable error occurred.
    NOT_INITED,

    // The service registered observers, timers, etc. and is ready to answer to
    // queries, fetch suggestions... Can change to states:
    // - DISABLED: when the global Chrome state changes, for example after
    //             |OnStateChanged| is called and sync is disabled.
    // - ERROR_OCCURRED: when an unrecoverable error occurred.
    READY,

    // The service is disabled and unregistered the related resources.
    // Can change to states:
    // - READY: when the global Chrome state changes, for example after
    //          |OnStateChanged| is called and sync is enabled.
    // - ERROR_OCCURRED: when an unrecoverable error occurred.
    DISABLED,

    // The service or one of its dependencies encountered an unrecoverable error
    // and the service can't be used anymore.
    ERROR_OCCURRED,

    COUNT
  };

  struct CategoryContent {
    // The current status of the category.
    CategoryStatus status = CategoryStatus::INITIALIZING;

    // The additional information about a category.
    CategoryInfo info;

    // True iff the server returned results in this category in the last fetch.
    // We never remove categories that the server still provides, but if the
    // server stops providing a category, we won't yet report it as NOT_PROVIDED
    // while we still have non-expired suggestions in it.
    bool included_in_last_server_response = true;

    // All currently active suggestions (excl. the dismissed ones).
    RemoteSuggestion::PtrVector suggestions;

    // All previous suggestions that we keep around in memory because they can
    // be on some open NTP. We do not persist this list so that on a new start
    // of Chrome, this is empty.
    // |archived| is a FIFO buffer with a maximum length.
    std::deque<std::unique_ptr<RemoteSuggestion>> archived;

    // Suggestions that the user dismissed. We keep these around until they
    // expire so we won't re-add them to |suggestions| on the next fetch.
    RemoteSuggestion::PtrVector dismissed;

    // Returns a non-dismissed suggestion with the given |id_within_category|,
    // or null if none exist.
    const RemoteSuggestion* FindSuggestion(
        const std::string& id_within_category) const;

    explicit CategoryContent(const CategoryInfo& info);
    CategoryContent(CategoryContent&&);
    ~CategoryContent();
    CategoryContent& operator=(CategoryContent&&);
  };

  // Fetches suggestions from the server and replaces old suggestions by the new
  // ones. Requests can be marked more important by setting
  // |interactive_request| to true (such request might circumvent the daily
  // quota for requests, etc.), useful for requests triggered by the user. After
  // the fetch finished, the provided |callback| will be triggered with the
  // status of the fetch.
  void FetchSuggestions(bool interactive_request,
                        std::unique_ptr<FetchStatusCallback> callback);

  // Returns the URL of the image of a suggestion if it is among the current or
  // among the archived suggestions in the matching category. Returns an empty
  // URL otherwise.
  GURL FindSuggestionImageUrl(const ContentSuggestion::ID& suggestion_id) const;

  // Callbacks for the RemoteSuggestionsDatabase.
  void OnDatabaseLoaded(RemoteSuggestion::PtrVector suggestions);
  void OnDatabaseError();

  // Callback for fetch-more requests with the RemoteSuggestionsFetcher.
  void OnFetchMoreFinished(
      const FetchDoneCallback& fetching_callback,
      Status status,
      RemoteSuggestionsFetcher::OptionalFetchedCategories fetched_categories);

  // Callback for regular fetch requests with the RemoteSuggestionsFetcher.
  void OnFetchFinished(
      std::unique_ptr<FetchStatusCallback> callback,
      bool interactive_request,
      Status status,
      RemoteSuggestionsFetcher::OptionalFetchedCategories fetched_categories);

  // Moves all suggestions from |to_archive| into the archive of the |content|.
  // Clears |to_archive|. As the archive is a FIFO buffer of limited size, this
  // function will also delete images from the database in case the associated
  // suggestion gets evicted from the archive.
  void ArchiveSuggestions(CategoryContent* content,
                          RemoteSuggestion::PtrVector* to_archive);

  // Sanitizes newly fetched suggestions -- e.g. adding missing dates and
  // filtering out incomplete results or dismissed suggestions (indicated by
  // |dismissed|).
  void SanitizeReceivedSuggestions(const RemoteSuggestion::PtrVector& dismissed,
                                   RemoteSuggestion::PtrVector* suggestions);

  // Adds newly available suggestions to |content|.
  void IntegrateSuggestions(CategoryContent* content,
                            RemoteSuggestion::PtrVector new_suggestions);

  // Dismisses a suggestion within a given category content.
  // Note that this modifies the suggestion datastructures of |content|
  // invalidating iterators.
  void DismissSuggestionFromCategoryContent(
      CategoryContent* content,
      const std::string& id_within_category);

  // Removes expired dismissed suggestions from the service and the database.
  void ClearExpiredDismissedSuggestions();

  // Removes images from the DB that are not referenced from any known
  // suggestion. Needs to iterate the whole suggestion database -- so do it
  // often enough to keep it small but not too often as it still iterates over
  // the file system.
  void ClearOrphanedImages();

  // Clears suggestions because any history item has been removed.
  void ClearHistoryDependentState();

  // Clears suggestions for any non-history related reason (e.g., sign-in status
  // change, etc.).
  void ClearSuggestions();

  // Clears all stored suggestions and updates the observer.
  void NukeAllSuggestions();

  // Completes the initialization phase of the service, registering the last
  // observers. This is done after construction, once the database is loaded.
  void FinishInitialization();

  // Triggers a state transition depending on the provided status. This method
  // is called when a change is detected by |status_service_|.
  void OnStatusChanged(RemoteSuggestionsStatus old_status,
                       RemoteSuggestionsStatus new_status);

  // Verifies state transitions (see |State|'s documentation) and applies them.
  // Also updates the provider status. Does nothing except updating the provider
  // status if called with the current state.
  void EnterState(State state);

  // Notifies the state change to ProviderStatusCallback specified by
  // SetProviderStatusCallback().
  void NotifyStateChanged();

  // Enables the service. Do not call directly, use |EnterState| instead.
  void EnterStateReady();

  // Disables the service. Do not call directly, use |EnterState| instead.
  void EnterStateDisabled();

  // Disables the service permanently because an unrecoverable error occurred.
  // Do not call directly, use |EnterState| instead.
  void EnterStateError();

  // Converts the cached suggestions in the given |category| to content
  // suggestions and notifies the observer.
  void NotifyNewSuggestions(Category category, const CategoryContent& content);

  // Updates the internal status for |category| to |category_status_| and
  // notifies the content suggestions observer if it changed.
  void UpdateCategoryStatus(Category category, CategoryStatus status);
  // Calls UpdateCategoryStatus() for all provided categories.
  void UpdateAllCategoryStatus(CategoryStatus status);

  // Updates the category info for |category|. If a corresponding
  // CategoryContent object does not exist, it will be created.
  // Returns the existing or newly created object.
  CategoryContent* UpdateCategoryInfo(Category category,
                                      const CategoryInfo& info);

  void RestoreCategoriesFromPrefs();
  void StoreCategoriesToPrefs();

  RequestParams BuildFetchParams() const;

  void MarkEmptyCategoriesAsLoading();

  State state_;

  PrefService* pref_service_;

  const Category articles_category_;

  std::map<Category, CategoryContent, Category::CompareByID> category_contents_;

  // The ISO 639-1 code of the language used by the application.
  const std::string application_language_code_;

  // Ranker that orders the categories. Not owned.
  CategoryRanker* category_ranker_;

  // The suggestions fetcher.
  std::unique_ptr<RemoteSuggestionsFetcher> suggestions_fetcher_;

  // The database for persisting suggestions.
  std::unique_ptr<RemoteSuggestionsDatabase> database_;
  base::TimeTicks database_load_start_;

  // The image fetcher.
  CachedImageFetcher image_fetcher_;

  // The service that provides events and data about the signin and sync state.
  std::unique_ptr<RemoteSuggestionsStatusService> status_service_;

  // Set to true if FetchSuggestions is called while the service isn't ready.
  // The fetch will be executed once the service enters the READY state.
  // TODO(jkrcal): create a struct and have here just one base::Optional<>?
  bool fetch_when_ready_;

  // The parameters for the fetch to perform later.
  bool fetch_when_ready_interactive_;
  std::unique_ptr<FetchStatusCallback> fetch_when_ready_callback_;

  RemoteSuggestionsScheduler* remote_suggestions_scheduler_;

  // Set to true if ClearHistoryDependentState is called while the service isn't
  // ready. The nuke will be executed once the service finishes initialization
  // or enters the READY state.
  bool clear_history_dependent_state_when_initialized_;

  // A clock for getting the time. This allows to inject a clock in tests.
  std::unique_ptr<base::Clock> clock_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsProviderImpl);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_IMPL_H_
