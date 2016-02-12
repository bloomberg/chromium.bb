// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_utils.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class OAuth2TokenService;
class SigninManagerBase;

namespace suggestions {

class BlacklistStore;
class SuggestionsStore;

// An interface to fetch server suggestions asynchronously.
class SuggestionsService : public KeyedService, public net::URLFetcherDelegate {
 public:
  using ResponseCallback = base::Callback<void(const SuggestionsProfile&)>;
  using BitmapCallback = base::Callback<void(const GURL&, const SkBitmap*)>;

  // Class taking ownership of |suggestions_store|, |thumbnail_manager| and
  // |blacklist_store|.
  SuggestionsService(
      const SigninManagerBase* signin_manager,
      OAuth2TokenService* token_service,
      net::URLRequestContextGetter* url_request_context,
      scoped_ptr<SuggestionsStore> suggestions_store,
      scoped_ptr<ImageManager> thumbnail_manager,
      scoped_ptr<BlacklistStore> blacklist_store);
  ~SuggestionsService() override;

  // Request suggestions data, which will be passed to |callback|. |sync_state|
  // will influence the behavior of this function (see SyncState definition).
  //
  // |sync_state| must be specified based on the current state of the system
  // (see suggestions::GetSyncState). Callers should call this function again if
  // sync state changes.
  //
  // If state allows for a network request, it is initiated unless a pending one
  // exists, to fill the cache for next time.
  void FetchSuggestionsData(SyncState sync_state,
                            const ResponseCallback& callback);

  // Retrieves stored thumbnail for website |url| asynchronously. Calls
  // |callback| with Bitmap pointer if found, and NULL otherwise.
  void GetPageThumbnail(const GURL& url, const BitmapCallback& callback);

  // A version of |GetPageThumbnail| that explicitly supplies the download URL
  // for the thumbnail. Replaces any pre-existing thumbnail URL with the
  // supplied one.
  void GetPageThumbnailWithURL(const GURL& url,
                               const GURL& thumbnail_url,
                               const BitmapCallback& callback);

  // Adds a URL to the blacklist cache, invoking |callback| on success or
  // |fail_callback| otherwise. The URL will eventually be uploaded to the
  // server.
  void BlacklistURL(const GURL& candidate_url,
                    const ResponseCallback& callback,
                    const base::Closure& fail_callback);

  // Removes a URL from the local blacklist, then invokes |callback|. If the URL
  // cannot be removed, the |fail_callback| is called.
  void UndoBlacklistURL(const GURL& url,
                        const ResponseCallback& callback,
                        const base::Closure& fail_callback);

  // Removes all URLs from the blacklist then invokes |callback|.
  void ClearBlacklist(const ResponseCallback& callback);

  // Determines which URL a blacklist request was for, irrespective of the
  // request's status. Returns false if |request| is not a blacklist request.
  static bool GetBlacklistedUrl(const net::URLFetcher& request, GURL* url);

  // Register SuggestionsService related prefs in the Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  friend class SuggestionsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest,
                           FetchSuggestionsDataNoAccessToken);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest,
                           IssueRequestIfNoneOngoingError);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest,
                           IssueRequestIfNoneOngoingResponseNotOK);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, BlacklistURL);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, BlacklistURLRequestFails);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, ClearBlacklist);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, UndoBlacklistURL);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, UndoBlacklistURLFailsHelper);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, GetBlacklistedUrl);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, UpdateBlacklistDelay);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, CheckDefaultTimeStamps);

  // Returns whether OAuth2 authentication is enabled. If false, cookies are
  // used for authentication.
  static bool UseOAuth2();

  // Helpers to build the various suggestions URLs. These are static members
  // rather than local functions in the .cc file to make them accessible to
  // tests.
  static GURL BuildSuggestionsURL();
  static std::string BuildSuggestionsBlacklistURLPrefix();
  static GURL BuildSuggestionsBlacklistURL(const GURL& candidate_url);
  static GURL BuildSuggestionsBlacklistClearURL();

  // Sets default timestamp for suggestions which do not have expiry timestamp.
  void SetDefaultExpiryTimestamp(SuggestionsProfile* suggestions,
                                 int64_t timestamp_usec);

  // Issues a network request if there isn't already one happening.
  void IssueRequestIfNoneOngoing(const GURL& url);

  // Issues a network request for suggestions (fetch, blacklist, or clear
  // blacklist, depending on |url|). |access_token| is used only if OAuth2
  // authentication is enabled.
  void IssueSuggestionsRequest(const GURL& url,
                               const std::string& access_token);

  // Creates a request to the suggestions service, properly setting headers.
  // If OAuth2 authentication is enabled, |access_token| should be a valid
  // OAuth2 access token, and will be written into an auth header.
  scoped_ptr<net::URLFetcher> CreateSuggestionsRequest(
      const GURL& url,
      const std::string& access_token);

  // net::URLFetcherDelegate implementation.
  // Called when fetch request completes. Parses the received suggestions data,
  // and dispatches them to callbacks stored in queue.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // KeyedService implementation.
  void Shutdown() override;

  // Loads the cached suggestions (or empty suggestions if no cache) and serves
  // the requestors with them.
  void ServeFromCache();

  // Applies the local blacklist to |suggestions|, then serves the requestors.
  void FilterAndServe(SuggestionsProfile* suggestions);

  // Schedules a blacklisting request if the local blacklist isn't empty.
  void ScheduleBlacklistUpload();

  // If the local blacklist isn't empty, picks a URL from it and issues a
  // blacklist request for it.
  void UploadOneFromBlacklist();

  // Updates |scheduling_delay_| based on the success of the last request.
  void UpdateBlacklistDelay(bool last_request_successful);

  // Adds extra data to suggestions profile.
  void PopulateExtraData(SuggestionsProfile* suggestions);

  // Test seams.
  base::TimeDelta blacklist_delay() const { return scheduling_delay_; }
  void set_blacklist_delay(base::TimeDelta delay) {
    scheduling_delay_ = delay; }

  base::ThreadChecker thread_checker_;

  net::URLRequestContextGetter* url_request_context_;

  // The cache for the suggestions.
  scoped_ptr<SuggestionsStore> suggestions_store_;

  // Used to obtain server thumbnails, if available.
  scoped_ptr<ImageManager> thumbnail_manager_;

  // The local cache for temporary blacklist, until uploaded to the server.
  scoped_ptr<BlacklistStore> blacklist_store_;

  // Delay used when scheduling a blacklisting task.
  base::TimeDelta scheduling_delay_;

  // Helper for fetching OAuth2 access tokens.
  class AccessTokenFetcher;
  scoped_ptr<AccessTokenFetcher> token_fetcher_;

  // Contains the current suggestions fetch request. Will only have a value
  // while a request is pending, and will be reset by |OnURLFetchComplete| or
  // if cancelled.
  scoped_ptr<net::URLFetcher> pending_request_;

  // The start time of the previous suggestions request. This is used to measure
  // the latency of requests. Initially zero.
  base::TimeTicks last_request_started_time_;

  // Queue of callbacks. These are flushed when fetch request completes.
  std::vector<ResponseCallback> waiting_requestors_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<SuggestionsService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsService);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
