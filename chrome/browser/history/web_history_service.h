// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_H_
#define CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

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
class WebHistoryService : public KeyedService {
 public:
  // Handles all the work of making an API request. This class encapsulates
  // the entire state of the request. When an instance is destroyed, all
  // aspects of the request are cancelled.
  class Request {
   public:
    virtual ~Request();

    // Returns true if the request is "pending" (i.e., it has been started, but
    // is not yet been complete).
    virtual bool is_pending() = 0;

   protected:
    Request();
  };

  // Callback with the result of a call to QueryHistory(). Currently, the
  // DictionaryValue is just the parsed JSON response from the server.
  // TODO(dubroy): Extract the DictionaryValue into a structured results object.
  typedef base::Callback<void(Request*, const base::DictionaryValue*)>
      QueryWebHistoryCallback;

  typedef base::Callback<void(bool success)> ExpireWebHistoryCallback;

  explicit WebHistoryService(Profile* profile);
  virtual ~WebHistoryService();

  // Searches synced history for visits matching |text_query|. The timeframe to
  // search, along with other options, is specified in |options|. If
  // |text_query| is empty, all visits in the timeframe will be returned.
  // This method is the equivalent of HistoryService::QueryHistory.
  // The caller takes ownership of the returned Request. If it is destroyed, the
  // request is cancelled.
  scoped_ptr<Request> QueryHistory(
      const base::string16& text_query,
      const QueryOptions& options,
      const QueryWebHistoryCallback& callback);

  // Removes all visits to specified URLs in specific time ranges.
  // This is the of equivalent HistoryService::ExpireHistory().
  void ExpireHistory(const std::vector<ExpireHistoryArgs>& expire_list,
                     const ExpireWebHistoryCallback& callback);

  // Removes all visits to specified URLs in the given time range.
  // This is the of equivalent HistoryService::ExpireHistoryBetween().
  void ExpireHistoryBetween(const std::set<GURL>& restrict_urls,
                            base::Time begin_time,
                            base::Time end_time,
                            const ExpireWebHistoryCallback& callback);

 private:
  // Called by |request| when a web history query has completed. Unpacks the
  // response and calls |callback|, which is the original callback that was
  // passed to QueryHistory().
  static void QueryHistoryCompletionCallback(
      const WebHistoryService::QueryWebHistoryCallback& callback,
      WebHistoryService::Request* request,
      bool success);

  // Called by |request| when a request to delete history from the server has
  // completed. Unpacks the response and calls |callback|, which is the original
  // callback that was passed to ExpireHistory().
  void ExpireHistoryCompletionCallback(
      const WebHistoryService::ExpireWebHistoryCallback& callback,
      WebHistoryService::Request* request,
      bool success);

  Profile* profile_;

  // Stores the version_info token received from the server in response to
  // a mutation operation (e.g., deleting history). This is used to ensure that
  // subsequent reads see a version of the data that includes the mutation.
  std::string server_version_info_;

  // Pending expiration requests to be canceled if not complete by profile
  // shutdown.
  std::set<Request*> pending_expire_requests_;

  base::WeakPtrFactory<WebHistoryService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebHistoryService);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_H_
