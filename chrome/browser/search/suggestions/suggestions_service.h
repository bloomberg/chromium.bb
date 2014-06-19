// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "chrome/browser/search/suggestions/thumbnail_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace suggestions {

class SuggestionsStore;

extern const char kSuggestionsFieldTrialName[];
extern const char kSuggestionsFieldTrialURLParam[];
extern const char kSuggestionsFieldTrialSuggestionsSuffixParam[];
extern const char kSuggestionsFieldTrialBlacklistSuffixParam[];
extern const char kSuggestionsFieldTrialStateParam[];
extern const char kSuggestionsFieldTrialControlParam[];
extern const char kSuggestionsFieldTrialStateEnabled[];

// An interface to fetch server suggestions asynchronously.
class SuggestionsService : public KeyedService, public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(const SuggestionsProfile&)> ResponseCallback;

  SuggestionsService(Profile* profile,
                     scoped_ptr<SuggestionsStore> suggestions_store);
  virtual ~SuggestionsService();

  // Whether this service is enabled.
  static bool IsEnabled();

  // Whether the user is part of a control group.
  static bool IsControlGroup();

  // Request suggestions data, which will be passed to |callback|. Initiates a
  // fetch request unless a pending one exists. To prevent multiple requests,
  // we place all |callback|s in a queue and update them simultaneously when
  // fetch request completes. Also posts a task to execute OnRequestTimeout
  // if the request hasn't completed in a given amount of time.
  void FetchSuggestionsData(ResponseCallback callback);

  // Similar to FetchSuggestionsData but doesn't post a task to execute
  // OnDelaySinceFetch.
  void FetchSuggestionsDataNoTimeout(ResponseCallback callback);

  // Retrieves stored thumbnail for website |url| asynchronously. Calls
  // |callback| with Bitmap pointer if found, and NULL otherwise.
  void GetPageThumbnail(
      const GURL& url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

  // Issue a blacklist request. If there is already a blacklist request
  // in flight, the new blacklist request is ignored.
  void BlacklistURL(const GURL& candidate_url, ResponseCallback callback);

  // Register SuggestionsService related prefs in the Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, FetchSuggestionsData);

  // Called to service the requestors if the issued suggestions request has
  // not completed in a given amount of time.
  virtual void OnRequestTimeout();

  // net::URLFetcherDelegate implementation.
  // Called when fetch request completes. Parses the received suggestions data,
  // and dispatches them to callbacks stored in queue.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Determines whether |request| is a blacklisting request.
  bool IsBlacklistRequest(net::URLFetcher* request) const;

  // Creates a request to the suggestions service, properly setting headers.
  net::URLFetcher* CreateSuggestionsRequest(const GURL& url);

  // Load the cached suggestions and service the requestors with them.
  void ServeFromCache();

  // The cache for the suggestions.
  scoped_ptr<SuggestionsStore> suggestions_store_;

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
  scoped_ptr<ThumbnailManager> thumbnail_manager_;

  Profile* profile_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<SuggestionsService> weak_ptr_factory_;

  // Timeout (in ms) before serving requestors after a fetch suggestions request
  // has been issued.
  int request_timeout_ms_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsService);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
