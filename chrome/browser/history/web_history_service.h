// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_H_
#define CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_H_

#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

namespace base {
class DictionaryValue;
}

namespace net {
class URLFetcher;
}

namespace history {

// Provides an API for querying Google servers for a signed-in user's
// synced history visits. It is roughly analogous to HistoryService, and
// supports a similar API.
class WebHistoryService : public ProfileKeyedService {
 public:
  // Handles all the work of making an API request. This class encapsulates
  // the entire state of the request. When an instance is destroyed, all
  // aspects of the request are cancelled.
  class Request {
   public:
    virtual ~Request();

   protected:
    Request();
  };

  // Callback with the result of a call to QueryHistory(). Currently, the
  // DictionaryValue is just the parsed JSON response from the server.
  // TODO(dubroy): Extract the DictionaryValue into a structured results object.
  typedef base::Callback<void(Request*, const DictionaryValue*)>
      QueryWebHistoryCallback;

  typedef base::Callback<void(Request*, bool success)>
      ExpireWebHistoryCallback;

  explicit WebHistoryService(Profile* profile);
  virtual ~WebHistoryService();

  // Searches synced history for visits matching |text_query|. The timeframe to
  // search, along with other options, is specified in |options|. If
  // |text_query| is empty, all visits in the timeframe will be returned.
  // This method is the equivalent of HistoryService::QueryHistory.
  // The caller takes ownership of the returned Request. If it is destroyed, the
  // request is cancelled.
  scoped_ptr<Request> QueryHistory(
      const string16& text_query,
      const QueryOptions& options,
      const QueryWebHistoryCallback& callback);

  // Deletes all visits to the given set of URLs between |begin_time| and
  // |end_time|.
  scoped_ptr<Request> ExpireHistoryBetween(
      const std::set<GURL>& restrict_urls,
      base::Time begin_time,
      base::Time end_time,
      const ExpireWebHistoryCallback& callback);

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(WebHistoryService);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_H_
