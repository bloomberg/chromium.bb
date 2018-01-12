// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_IMPL_H_
#define COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
#include "url/gurl.h"

namespace identity {
class IdentityManager;
}  // namespace identity

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace syncer {
class SyncService;
}  // namespace syncer

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace suggestions {

class BlacklistStore;
class ImageManager;
class SuggestionsStore;

// Actual (non-test) implementation of the SuggestionsService interface.
class SuggestionsServiceImpl : public SuggestionsService,
                               public net::URLFetcherDelegate,
                               public syncer::SyncServiceObserver {
 public:
  SuggestionsServiceImpl(identity::IdentityManager* identity_manager,
                         syncer::SyncService* sync_service,
                         net::URLRequestContextGetter* url_request_context,
                         std::unique_ptr<SuggestionsStore> suggestions_store,
                         std::unique_ptr<ImageManager> thumbnail_manager,
                         std::unique_ptr<BlacklistStore> blacklist_store,
                         std::unique_ptr<base::TickClock> tick_clock);
  ~SuggestionsServiceImpl() override;

  // SuggestionsService implementation.
  bool FetchSuggestionsData() override;
  base::Optional<SuggestionsProfile> GetSuggestionsDataFromCache()
      const override;
  std::unique_ptr<ResponseCallbackList::Subscription> AddCallback(
      const ResponseCallback& callback) override WARN_UNUSED_RESULT;
  void GetPageThumbnail(const GURL& url,
                        const BitmapCallback& callback) override;
  void GetPageThumbnailWithURL(const GURL& url,
                               const GURL& thumbnail_url,
                               const BitmapCallback& callback) override;
  bool BlacklistURL(const GURL& candidate_url) override;
  bool UndoBlacklistURL(const GURL& url) override;
  void ClearBlacklist() override;

  base::TimeDelta BlacklistDelayForTesting() const;
  bool HasPendingRequestForTesting() const;

  // Determines which URL a blacklist request was for, irrespective of the
  // request's status. Returns false if |request| is not a blacklist request.
  static bool GetBlacklistedUrl(const net::URLFetcher& request, GURL* url);

  // Register SuggestionsService related prefs in the Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // Establishes the different sync states that matter to SuggestionsService.
  enum SyncState {
    // State: Sync service is not initialized, yet not disabled. History sync
    //     state is unknown (since not initialized).
    // Behavior: Do not issue server requests, but serve from cache if
    //     available.
    NOT_INITIALIZED_ENABLED,

    // State: Sync service is initialized, sync is enabled and history sync is
    //     enabled.
    // Behavior: Update suggestions from the server on FetchSuggestionsData().
    INITIALIZED_ENABLED_HISTORY,

    // State: Sync service is disabled or history sync is disabled.
    // Behavior: Do not issue server requests. Clear the cache. Serve empty
    //     suggestions.
    SYNC_OR_HISTORY_SYNC_DISABLED,
  };

  // The action that should be taken as the result of a RefreshSyncState call.
  enum RefreshAction { NO_ACTION, FETCH_SUGGESTIONS, CLEAR_SUGGESTIONS };

  // Helpers to build the various suggestions URLs. These are static members
  // rather than local functions in the .cc file to make them accessible to
  // tests.
  static GURL BuildSuggestionsURL();
  static std::string BuildSuggestionsBlacklistURLPrefix();
  static GURL BuildSuggestionsBlacklistURL(const GURL& candidate_url);
  static GURL BuildSuggestionsBlacklistClearURL();

  // Computes the appropriate SyncState from |sync_service_|.
  SyncState ComputeSyncState() const;

  // Re-computes |sync_state_| from the sync service. Returns the action that
  // should be taken in response.
  RefreshAction RefreshSyncState() WARN_UNUSED_RESULT;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

  // Sets default timestamp for suggestions which do not have expiry timestamp.
  void SetDefaultExpiryTimestamp(SuggestionsProfile* suggestions,
                                 int64_t timestamp_usec);

  // Issues a network request if there isn't already one happening.
  void IssueRequestIfNoneOngoing(const GURL& url);

  // Called when an access token request completes (successfully or not).
  void AccessTokenAvailable(const GURL& url,
                            const GoogleServiceAuthError& error,
                            const std::string& access_token);

  // Issues a network request for suggestions (fetch, blacklist, or clear
  // blacklist, depending on |url|).
  void IssueSuggestionsRequest(const GURL& url,
                               const std::string& access_token);

  // Creates a request to the suggestions service, properly setting headers.
  // If OAuth2 authentication is enabled, |access_token| should be a valid
  // OAuth2 access token, and will be written into an auth header.
  std::unique_ptr<net::URLFetcher> CreateSuggestionsRequest(
      const GURL& url,
      const std::string& access_token);

  // net::URLFetcherDelegate implementation.
  // Called when fetch request completes. Parses the received suggestions data,
  // and dispatches them to callbacks stored in queue.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // KeyedService implementation.
  void Shutdown() override;

  // Schedules a blacklisting request if the local blacklist isn't empty.
  void ScheduleBlacklistUpload();

  // If the local blacklist isn't empty, picks a URL from it and issues a
  // blacklist request for it.
  void UploadOneFromBlacklist();

  // Adds extra data to suggestions profile.
  void PopulateExtraData(SuggestionsProfile* suggestions);

  base::ThreadChecker thread_checker_;

  identity::IdentityManager* identity_manager_;

  syncer::SyncService* sync_service_;
  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observer_;

  SyncState sync_state_;

  net::URLRequestContextGetter* url_request_context_;

  // The cache for the suggestions.
  std::unique_ptr<SuggestionsStore> suggestions_store_;

  // Used to obtain server thumbnails, if available.
  std::unique_ptr<ImageManager> thumbnail_manager_;

  // The local cache for temporary blacklist, until uploaded to the server.
  std::unique_ptr<BlacklistStore> blacklist_store_;

  std::unique_ptr<base::TickClock> tick_clock_;

  // Backoff for scheduling blacklist upload tasks.
  net::BackoffEntry blacklist_upload_backoff_;

  base::OneShotTimer blacklist_upload_timer_;

  // Helper for fetching OAuth2 access tokens. This is non-null iff an access
  // token request is currently in progress.
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  // Contains the current suggestions fetch request. Will only have a value
  // while a request is pending, and will be reset by |OnURLFetchComplete| or
  // if cancelled.
  std::unique_ptr<net::URLFetcher> pending_request_;

  // The start time of the previous suggestions request. This is used to measure
  // the latency of requests. Initially zero.
  base::TimeTicks last_request_started_time_;

  ResponseCallbackList callback_list_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<SuggestionsServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsServiceImpl);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_IMPL_H_
