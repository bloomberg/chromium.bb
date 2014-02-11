// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_H_

#include <vector>

#include "base/basictypes.h"
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

// Provides an interface for the Suggestions component that provides server
// suggestions.
class SuggestionsService : public BrowserContextKeyedService,
                           public net::URLFetcherDelegate {
 public:
  explicit SuggestionsService(Profile* profile);
  virtual ~SuggestionsService();

  // Whether this service is enabled.
  static bool IsEnabled();

  const SuggestionsProfile& suggestions() { return suggestions_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(SuggestionsServiceTest, FetchSuggestionsData);

  // Starts the fetching process once, where |OnURLFetchComplete| is called with
  // the response.
  void FetchSuggestionsData();

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Contains the current suggestions request. Will only have a value while a
  // request is pending, and will be reset by |OnURLFetchComplete|.
  scoped_ptr<net::URLFetcher> pending_request_;

  // The start time of the last suggestions request. This is used to measure the
  // latency of requests. Initially zero.
  base::TimeTicks last_request_started_time_;

  // The URL to fetch suggestions data from.
  GURL suggestions_url_;

  // Stores the suggestions as they were received from the server.
  SuggestionsProfile suggestions_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsService);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_H_
