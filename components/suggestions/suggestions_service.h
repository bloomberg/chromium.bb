// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_utils.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace suggestions {

class BlacklistStore;
class SuggestionsStore;

extern const char kSuggestionsFieldTrialName[];
extern const char kSuggestionsFieldTrialURLParam[];
extern const char kSuggestionsFieldTrialCommonParamsParam[];
extern const char kSuggestionsFieldTrialBlacklistPathParam[];
extern const char kSuggestionsFieldTrialBlacklistUrlParam[];
extern const char kSuggestionsFieldTrialStateParam[];
extern const char kSuggestionsFieldTrialControlParam[];
extern const char kSuggestionsFieldTrialStateEnabled[];
extern const int64 kDefaultExpiryUsec;

// An interface to fetch server suggestions asynchronously.
class SuggestionsService : public KeyedService, public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(const SuggestionsProfile&)> ResponseCallback;

  SuggestionsService(
      net::URLRequestContextGetter* url_request_context,
      scoped_ptr<SuggestionsStore> suggestions_store,
      scoped_ptr<ImageManager> thumbnail_manager,
      scoped_ptr<BlacklistStore> blacklist_store);
  virtual ~SuggestionsService();

  // Whether this service is enabled.
  static bool IsEnabled();

  // Whether the user is part of a control group.
  static bool IsControlGroup();

  // Request suggestions data, which will be passed to |callback|. |sync_state|
  // will influence the behavior of this function (see SyncState definition).
  //
  // |sync_state| must be specified based on the current state of the system
  // (see suggestions::GetSyncState). Callers should call this function again if
  // sync state changes.
  //
  // If state allows for a network request, it is initiated unless a pending one
  // exists. To prevent multiple requests, all |callback|s are placed in a queue
  // and are updated simultaneously when the fetch completes. Also posts a task
  // to execute OnRequestTimeout if the request hasn't completed in a given
  // amount of time.
  void FetchSuggestionsData(SyncState sync_state,
                            ResponseCallback callback);

  // Retrieves stored thumbnail for website |url| asynchronously. Calls
  // |callback| with Bitmap pointer if found, and NULL otherwise.
  void GetPageThumbnail(
      const GURL& url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

  // Issue a blacklist request. If there is already a blacklist request
  // in flight, the new blacklist request is ignored.
  void BlacklistURL(const GURL& candidate_url,
                    const ResponseCallback& callback);

  // Determines which URL a blacklist request was for, irrespective of the
  // request's status. Returns false if |request| is not a blacklist request.
  static bool GetBlacklistedUrl(const net::URLFetcher& request, GURL* url);

  // Register SuggestionsService related prefs in the Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Sets default timestamp for suggestions which do not have expiry timestamp.
  void SetDefaultExpiryTimestamp(SuggestionsProfile* suggestions,
                                 int64 timestamp_usec);
 private:
  friend class SuggestionsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, BlacklistURLFails);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, FetchSuggestionsData);
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, UpdateBlacklistDelay);

  // Similar to FetchSuggestionsData but doesn't post a task to execute
  // OnDelaySinceFetch.
  void FetchSuggestionsDataNoTimeout(ResponseCallback callback);

  // Issue a request.
  void IssueRequest(const GURL& url);

  // Creates a request to the suggestions service, properly setting headers.
  net::URLFetcher* CreateSuggestionsRequest(const GURL& url);

  // Called to service the requestors if the issued suggestions request has
  // not completed in a given amount of time.
  virtual void OnRequestTimeout();

  // net::URLFetcherDelegate implementation.
  // Called when fetch request completes. Parses the received suggestions data,
  // and dispatches them to callbacks stored in queue.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Load the cached suggestions and service the requestors with them.
  void ServeFromCache();

  // Apply the local blacklist to |suggestions|, then serve the requestors.
  void FilterAndServe(SuggestionsProfile* suggestions);

  // Schedule a blacklisting request if the local blacklist isn't empty.
  // |last_request_successful| is used for exponentially backing off when
  // requests fail.
  void ScheduleBlacklistUpload(bool last_request_successful);

  // If the local blacklist isn't empty, pick a URL from it and issue a
  // blacklist request for it.
  void UploadOneFromBlacklist();

  // Updates |blacklist_delay_sec_| based on the success of the last request.
  void UpdateBlacklistDelay(bool last_request_successful);

  // Test seams.
  int blacklist_delay() const { return blacklist_delay_sec_; }
  void set_blacklist_delay(int delay) { blacklist_delay_sec_ = delay; }

  base::ThreadChecker thread_checker_;

  // The cache for the suggestions.
  scoped_ptr<SuggestionsStore> suggestions_store_;

  // The local cache for temporary blacklist, until uploaded to the server.
  scoped_ptr<BlacklistStore> blacklist_store_;

  // Contains the current suggestions fetch request. Will only have a value
  // while a request is pending, and will be reset by |OnURLFetchComplete|.
  scoped_ptr<net::URLFetcher> pending_request_;

  // A closure that is run on a timeout from issuing the suggestions fetch
  // request, if the request hasn't completed.
  scoped_ptr<base::CancelableClosure> pending_timeout_closure_;

  // The start time of the previous suggestions request. This is used to measure
  // the latency of requests. Initially zero.
  base::TimeTicks last_request_started_time_;

  // The URL to fetch suggestions data from.
  GURL suggestions_url_;

  // Prefix for building the blacklisting URL.
  std::string blacklist_url_prefix_;

  // Queue of callbacks. These are flushed when fetch request completes.
  std::vector<ResponseCallback> waiting_requestors_;

  // Used to obtain server thumbnails, if available.
  scoped_ptr<ImageManager> thumbnail_manager_;

  net::URLRequestContextGetter* url_request_context_;

  // Delay used when scheduling a blacklisting task.
  int blacklist_delay_sec_;

  // Timeout (in ms) before serving requestors after a fetch suggestions request
  // has been issued.
  int request_timeout_ms_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<SuggestionsService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsService);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
