// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_scheduler.h"
#include "components/ntp_snippets/ntp_snippets_status_service.h"
#include "components/suggestions/suggestions_service.h"
#include "components/sync_driver/sync_service_observer.h"

class PrefRegistrySimple;
class PrefService;
class SigninManagerBase;

namespace base {
class RefCountedMemory;
class Value;
}

namespace gfx {
class Image;
}

namespace image_fetcher {
class ImageDecoder;
class ImageFetcher;
}

namespace suggestions {
class SuggestionsProfile;
}

namespace sync_driver {
class SyncService;
}

namespace ntp_snippets {

class NTPSnippetsDatabase;
class NTPSnippetsServiceObserver;

// Retrieves fresh content data (articles) from the server, stores them and
// provides them as content suggestions.
// TODO(pke): Rename this service to ArticleSuggestionsProvider and move to
// a subdirectory.
class NTPSnippetsService : public image_fetcher::ImageFetcherDelegate,
                           public ContentSuggestionsProvider {
 public:
  // |application_language_code| should be a ISO 639-1 compliant string, e.g.
  // 'en' or 'en-US'. Note that this code should only specify the language, not
  // the locale, so 'en_US' (English language with US locale) and 'en-GB_US'
  // (British English person in the US) are not language codes.
  NTPSnippetsService(Observer* observer,
                     CategoryFactory* category_factory,
                     PrefService* pref_service,
                     suggestions::SuggestionsService* suggestions_service,
                     const std::string& application_language_code,
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

  // Fetches snippets from the server and adds them to the current ones.
  // Requests can be marked more important by setting |force_request| to true
  // (such request might circumvent the daily quota for requests, etc.) Useful
  // for requests triggered by the user.
  void FetchSnippets(bool force_request);

  // Fetches snippets from the server for specified hosts (overriding
  // suggestions from the suggestion service) and adds them to the current ones.
  // Only called from chrome://snippets-internals, DO NOT USE otherwise!
  // Ignored while |loaded()| is false.
  void FetchSnippetsFromHosts(const std::set<std::string>& hosts,
                              bool force_request);

  // Available snippets.
  const NTPSnippet::PtrVector& snippets() const { return snippets_; }

  // Returns the list of snippets previously dismissed by the user (that are
  // not expired yet).
  const NTPSnippet::PtrVector& dismissed_snippets() const {
    return dismissed_snippets_;
  }

  const NTPSnippetsFetcher* snippets_fetcher() const {
    return snippets_fetcher_.get();
  }

  // Returns a reason why the service is disabled, or DisabledReason::NONE
  // if it's not.
  DisabledReason disabled_reason() const {
    return snippets_status_service_->disabled_reason();
  }

  // (Re)schedules the periodic fetching of snippets. This is necessary because
  // the schedule depends on the time of day.
  void RescheduleFetching();

  // ContentSuggestionsProvider implementation
  // TODO(pke): At some point reorder the implementations in the .cc file
  // accordingly.
  std::vector<Category> GetProvidedCategories() override;
  CategoryStatus GetCategoryStatus(Category category) override;
  void DismissSuggestion(const std::string& suggestion_id) override;
  void FetchSuggestionImage(const std::string& suggestion_id,
                            const ImageFetchedCallback& callback) override;
  void ClearCachedSuggestionsForDebugging() override;
  void ClearDismissedSuggestionsForDebugging() override;

  // Returns the lists of suggestion hosts the snippets are restricted to.
  std::set<std::string> GetSuggestionsHosts() const;

  // Returns the maximum number of snippets that will be shown at once.
  static int GetMaxSnippetCountForTesting();

 private:
  friend class NTPSnippetsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsServiceTest, StatusChanges);

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

  // image_fetcher::ImageFetcherDelegate implementation.
  void OnImageDataFetched(const std::string& snippet_id,
                          const std::string& image_data) override;

  // Callbacks for the NTPSnippetsDatabase.
  void OnDatabaseLoaded(NTPSnippet::PtrVector snippets);
  void OnDatabaseError();

  // Callback for the SuggestionsService.
  void OnSuggestionsChanged(const suggestions::SuggestionsProfile& suggestions);

  // Callback for the NTPSnippetsFetcher.
  void OnFetchFinished(NTPSnippetsFetcher::OptionalSnippets snippets);

  // Merges newly available snippets with the previously available list.
  void MergeSnippets(NTPSnippet::PtrVector new_snippets);

  std::set<std::string> GetSnippetHostsFromPrefs() const;
  void StoreSnippetHostsToPrefs(const std::set<std::string>& hosts);

  // Removes the expired snippets (including dismissed) from the service and the
  // database, and schedules another pass for the next expiration.
  void ClearExpiredSnippets();

  // Completes the initialization phase of the service, registering the last
  // observers. This is done after construction, once the database is loaded.
  void FinishInitialization();

  void OnSnippetImageFetchedFromDatabase(const std::string& snippet_id,
                                         const ImageFetchedCallback& callback,
                                         std::string data);

  void OnSnippetImageDecoded(const std::string& snippet_id,
                             const ImageFetchedCallback& callback,
                             const gfx::Image& image);

  void FetchSnippetImageFromNetwork(const std::string& snippet_id,
                                    const ImageFetchedCallback& callback);

  // Triggers a state transition depending on the provided reason to be
  // disabled (or lack thereof). This method is called when a change is detected
  // by |snippets_status_service_|.
  void OnDisabledReasonChanged(DisabledReason disabled_reason);

  // Verifies state transitions (see |State|'s documentation) and applies them.
  // Also updates the provider status. Does nothing except updating the provider
  // status if called with the current state.
  void EnterState(State state, CategoryStatus status);

  // Enables the service and triggers a fetch if required. Do not call directly,
  // use |EnterState| instead.
  void EnterStateEnabled(bool fetch_snippets);

  // Disables the service. Do not call directly, use |EnterState| instead.
  void EnterStateDisabled();

  // Disables the service permanently because an unrecoverable error occurred.
  // Do not call directly, use |EnterState| instead.
  void EnterStateError();

  // Converts the cached snippets to article content suggestions and notifies
  // the observers.
  void NotifyNewSuggestions();

  // Updates the internal status |category_status_| and notifies the content
  // suggestions observer if it changed.
  void UpdateCategoryStatus(CategoryStatus status);

  State state_;

  CategoryStatus category_status_;

  PrefService* pref_service_;

  suggestions::SuggestionsService* suggestions_service_;

  // All current suggestions (i.e. not dismissed ones).
  NTPSnippet::PtrVector snippets_;

  // Suggestions that the user dismissed. We keep these around until they expire
  // so we won't re-add them on the next fetch.
  NTPSnippet::PtrVector dismissed_snippets_;

  // The ISO 639-1 code of the language used by the application.
  const std::string application_language_code_;

  // Scheduler for fetching snippets. Not owned.
  NTPSnippetsScheduler* scheduler_;

  // The subscription to the SuggestionsService. When the suggestions change,
  // SuggestionsService will call |OnSuggestionsChanged|, which triggers an
  // update to the set of snippets.
  using SuggestionsSubscription =
      suggestions::SuggestionsService::ResponseCallbackList::Subscription;
  std::unique_ptr<SuggestionsSubscription> suggestions_service_subscription_;

  // The snippets fetcher.
  std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher_;

  // Timer that calls us back when the next snippet expires.
  base::OneShotTimer expiry_timer_;

  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;

  // The database for persisting snippets.
  std::unique_ptr<NTPSnippetsDatabase> database_;

  // The service that provides events and data about the signin and sync state.
  std::unique_ptr<NTPSnippetsStatusService> snippets_status_service_;

  // Set to true if FetchSnippets is called before the database has been loaded.
  // The fetch will be executed after the database load finishes.
  bool fetch_after_load_;

  const Category provided_category_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsService);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_
