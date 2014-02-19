// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class Profile;

namespace suggestions {

extern const char kSuggestionsFieldTrialName[];
extern const char kSuggestionsFieldTrialURLParam[];
extern const char kSuggestionsFieldTrialStateParam[];
extern const char kSuggestionsFieldTrialStateEnabled[];

// An interface to fetch server suggestions asynchronously.
class SuggestionsService : public BrowserContextKeyedService,
                           public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(const SuggestionsProfile&)> ResponseCallback;

  explicit SuggestionsService(Profile* profile);
  virtual ~SuggestionsService();

  // Whether this service is enabled.
  static bool IsEnabled();

  // Request suggestions data, which will be passed to |callback|. Initiates a
  // fetch request unless a pending one exists. To prevent multiple requests,
  // we place all |callback|s in a queue and update them simultaneously when
  // fetch request completes.
  void FetchSuggestionsData(ResponseCallback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, FetchSuggestionsData);

  // net::URLFetcherDelegate implementation.
  // Called when fetch request completes. Parses the received suggestions data,
  // and dispatches them to callbacks stored in queue.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Contains the current suggestions fetch request. Will only have a value
  // while a request is pending, and will be reset by |OnURLFetchComplete|.
  scoped_ptr<net::URLFetcher> pending_request_;

  // The start time of the previous suggestions request. This is used to measure
  // the latency of requests. Initially zero.
  base::TimeTicks last_request_started_time_;

  // The URL to fetch suggestions data from.
  GURL suggestions_url_;

  // Queue of callbacks. These are flushed when fetch request completes.
  std::vector<ResponseCallback> waiting_requestors_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsService);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
