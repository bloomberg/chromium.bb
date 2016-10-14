// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_SERVICE_H_

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/remote/ntp_snippet.h"
#include "components/ntp_snippets/remote/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/remote/ntp_snippets_scheduler.h"
#include "components/ntp_snippets/remote/ntp_snippets_status_service.h"
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

class NTPSnippetsDatabase;
class UserClassifier;

// Retrieves fresh content data (articles) from the server, stores them and
// provides them as content suggestions.
// This class is final because it does things in its constructor which make it
// unsafe to derive from it.
// TODO(treib): Introduce two-phase initialization and make the class not final?
// TODO(pke): Rename this service to ArticleSuggestionsProvider and move to
// a subdirectory.
// TODO(jkrcal): this class grows really, really large. The fact that
// NTPSnippetService also implements ImageFetcherDelegate adds unnecessary
// complexity (and after all the Service is conceptually not an
// ImagerFetcherDeletage ;-)). Instead, the cleaner solution would  be to define
// a CachedImageFetcher class that handles the caching aspects and looks like an
// image fetcher to the NTPSnippetService.
class NTPSnippetsService final : public ContentSuggestionsProvider,
                                 public image_fetcher::ImageFetcherDelegate {
 public:
  // |application_language_code| should be a ISO 639-1 compliant string, e.g.
  // 'en' or 'en-US'. Note that this code should only specify the language, not
  // the locale, so 'en_US' (English language with US locale) and 'en-GB_US'
  // (British English person in the US) are not language codes.
  NTPSnippetsService(Observer* observer,
                     CategoryFactory* category_factory,
                     PrefService* pref_service,
                     const std::string& application_language_code,
                     const UserClassifier* user_classifier,
                     NTPSnippetsScheduler* scheduler,
                     std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher,
                     std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
                     std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
                     std::unique_ptr<NTPSnippetsDatabase> database,
                     std::unique_ptr<NTPSnippetsStatusService> status_service);

  ~NTPSnippetsService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns whether the service is ready. While this is false, the list of
  // snippets will be empty, and all modifications to it (fetch, dismiss, etc)
  // will be ignored.
  bool ready() const { return state_ == State::READY; }

  // Returns whether the service is initialized. While this is false, some
  // calls may trigger DCHECKs.
  bool initialized() const { return ready() || state_ == State::DISABLED; }

  // Fetches snippets from the server and replaces old snippets by the new ones.
  // Requests can be marked more important by setting |interactive_request| to
  // true (such request might circumvent the daily quota for requests, etc.)
  // Useful for requests triggered by the user.
  void FetchSnippets(bool interactive_request);

  // Fetches snippets from the server for specified hosts and adds them to the
  // current ones. Only called from chrome://snippets-internals, DO NOT USE
  // otherwise! Ignored while ready() is false.
  void FetchSnippetsFromHosts(const std::set<std::string>& hosts,
                              bool interactive_request);

  const NTPSnippetsFetcher* snippets_fetcher() const {
    return snippets_fetcher_.get();
  }

  // (Re)schedules the periodic fetching of snippets. If |force| is true, the
  // tasks will be re-scheduled even if they already exist and have the correct
  // periods.
  void RescheduleFetching(bool force);

  // ContentSuggestionsProvider implementation
  CategoryStatus GetCategoryStatus(Category category) override;
  CategoryInfo GetCategoryInfo(Category category) override;
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id) override;
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            const ImageFetchedCallback& callback) override;
  void ClearHistory(
      base::Time begin,
      base::Time end,
      const base::Callback<bool(const GURL& url)>& filter) override;
  void ClearCachedSuggestions(Category category) override;
  void GetDismissedSuggestionsForDebugging(
      Category category,
      const DismissedSuggestionsCallback& callback) override;
  void ClearDismissedSuggestionsForDebugging(Category category) override;

  // Returns the maximum number of snippets that will be shown at once.
  static int GetMaxSnippetCountForTesting();

  // Available snippets, only for unit tests.
  // TODO(treib): Get rid of this. Tests should use a fake observer instead.
  const NTPSnippet::PtrVector& GetSnippetsForTesting(Category category) const {
    return categories_.find(category)->second.snippets;
  }

  // Available snippets, only for unit tests.
  const NTPSnippet::PtrVector& GetArchivedSnippetsForTesting(
      Category category) const {
    return categories_.find(category)->second.archived;
  }

  // Dismissed snippets, only for unit tests.
  const NTPSnippet::PtrVector& GetDismissedSnippetsForTesting(
      Category category) const {
    return categories_.find(category)->second.dismissed;
  }

 private:
  friend class NTPSnippetsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsServiceTest,
                           RemoveExpiredDismissedContent);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsServiceTest, RescheduleOnStateChange);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsServiceTest, StatusChanges);
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsServiceTest,
                           SuggestionsFetchedOnSignInAndSignOut);

  // Possible state transitions:
  //       NOT_INITED --------+
  //       /       \          |
  //      v         v         |
  //   READY <--> DISABLED    |
  //       \       /          |
  //        v     v           |
  //     ERROR_OCCURRED <-----+
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
    // queries, fetch snippets... Can change to states:
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
    ERROR_OCCURRED
  };

  // Returns the URL of the image of a snippet if it is among the current or
  // among the archived snippets in the matching category. Returns an empty URL
  // otherwise.
  GURL FindSnippetImageUrl(const ContentSuggestion::ID& suggestion_id) const;

  // image_fetcher::ImageFetcherDelegate implementation.
  void OnImageDataFetched(const std::string& id_within_category,
                          const std::string& image_data) override;

  // Callbacks for the NTPSnippetsDatabase.
  void OnDatabaseLoaded(NTPSnippet::PtrVector snippets);
  void OnDatabaseError();

  // Callback for the NTPSnippetsFetcher.
  void OnFetchFinished(
      NTPSnippetsFetcher::OptionalFetchedCategories fetched_categories);

  // Moves all snippets from |to_archive| into the archive of the |category|.
  // It also deletes the snippets from the DB and keeps the archive reasonably
  // short.
  void ArchiveSnippets(Category category, NTPSnippet::PtrVector* to_archive);

  // Replace old snippets in |category| by newly available snippets.
  void ReplaceSnippets(Category category, NTPSnippet::PtrVector new_snippets);

  // Removes expired dismissed snippets from the service and the database.
  void ClearExpiredDismissedSnippets();

  // Removes images from the DB that are not referenced from any known snippet.
  // Needs to iterate the whole snippet database -- so do it often enough to
  // keep it small but not too often as it still iterates over the file system.
  void ClearOrphanedImages();

  // Clears all stored snippets and updates the observer.
  void NukeAllSnippets();

  // Completes the initialization phase of the service, registering the last
  // observers. This is done after construction, once the database is loaded.
  void FinishInitialization();

  void OnSnippetImageFetchedFromDatabase(
      const ImageFetchedCallback& callback,
      const ContentSuggestion::ID& suggestion_id,
      std::string data);

  void OnSnippetImageDecodedFromDatabase(
      const ImageFetchedCallback& callback,
      const ContentSuggestion::ID& suggestion_id,
      const gfx::Image& image);

  void FetchSnippetImageFromNetwork(const ContentSuggestion::ID& suggestion_id,
                                    const ImageFetchedCallback& callback);

  void OnSnippetImageDecodedFromNetwork(const ImageFetchedCallback& callback,
                                        const std::string& id_within_category,
                                        const gfx::Image& image);

  // Triggers a state transition depending on the provided snippets status. This
  // method is called when a change is detected by |snippets_status_service_|.
  void OnSnippetsStatusChanged(SnippetsStatus old_snippets_status,
                               SnippetsStatus new_snippets_status);

  // Verifies state transitions (see |State|'s documentation) and applies them.
  // Also updates the provider status. Does nothing except updating the provider
  // status if called with the current state.
  void EnterState(State state);

  // Enables the service. Do not call directly, use |EnterState| instead.
  void EnterStateReady();

  // Disables the service. Do not call directly, use |EnterState| instead.
  void EnterStateDisabled();

  // Disables the service permanently because an unrecoverable error occurred.
  // Do not call directly, use |EnterState| instead.
  void EnterStateError();

  // Converts the cached snippets to article content suggestions and notifies
  // the observers.
  void NotifyNewSuggestions();

  // Updates the internal status for |category| to |category_status_| and
  // notifies the content suggestions observer if it changed.
  void UpdateCategoryStatus(Category category, CategoryStatus status);
  // Calls UpdateCategoryStatus() for all provided categories.
  void UpdateAllCategoryStatus(CategoryStatus status);

  void RestoreCategoriesFromPrefs();
  void StoreCategoriesToPrefs();

  State state_;

  PrefService* pref_service_;

  const Category articles_category_;

  // TODO(sfiera): Reduce duplication of CategoryContent with CategoryInfo.
  struct CategoryContent {
    CategoryStatus status = CategoryStatus::INITIALIZING;

    // The title of the section, localized to the running UI language.
    base::string16 localized_title;

    // True iff the server returned results in this category in the last fetch.
    // We never remove categories that the server still provides, but if the
    // server stops providing a category, we won't yet report it as NOT_PROVIDED
    // while we still have non-expired snippets in it.
    bool provided_by_server = true;

    // All currently active suggestions (excl. the dismissed ones).
    NTPSnippet::PtrVector snippets;

    // All previous suggestions that we keep around in memory because they can
    // be on some open NTP. We do not persist this list so that on a new start
    // of Chrome, this is empty.
    NTPSnippet::PtrVector archived;

    // Suggestions that the user dismissed. We keep these around until they
    // expire so we won't re-add them to |snippets| on the next fetch.
    NTPSnippet::PtrVector dismissed;

    // Returns a non-dismissed snippet with the given |id_within_category|, or
    // null if none exist.
    const NTPSnippet* FindSnippet(const std::string& id_within_category) const;

    CategoryContent();
    CategoryContent(CategoryContent&&);
    ~CategoryContent();
    CategoryContent& operator=(CategoryContent&&);
  };
  std::map<Category, CategoryContent, Category::CompareByID> categories_;

  // The ISO 639-1 code of the language used by the application.
  const std::string application_language_code_;

  // Classifier that tells us how active the user is. Not owned.
  const UserClassifier* user_classifier_;

  // Scheduler for fetching snippets. Not owned.
  NTPSnippetsScheduler* scheduler_;

  // The snippets fetcher.
  std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher_;

  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;

  // The database for persisting snippets.
  std::unique_ptr<NTPSnippetsDatabase> database_;

  // The service that provides events and data about the signin and sync state.
  std::unique_ptr<NTPSnippetsStatusService> snippets_status_service_;

  // Set to true if FetchSnippets is called while the service isn't ready.
  // The fetch will be executed once the service enters the READY state.
  bool fetch_when_ready_;

  // Set to true if NukeAllSnippets is called while the service isn't ready.
  // The nuke will be executed once the service finishes initialization or
  // enters the READY state.
  bool nuke_when_initialized_;

  // Request throttler for limiting requests to thumbnail images.
  RequestThrottler thumbnail_requests_throttler_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsService);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_SERVICE_H_
