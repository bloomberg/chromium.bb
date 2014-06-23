// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Structs that hold data used in broadcasting notifications.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__
#define CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__

#include <set>

#include "chrome/browser/history/history_details.h"
#include "chrome/browser/history/history_types.h"
#include "components/history/core/browser/keyword_id.h"
#include "url/gurl.h"

namespace history {

// Details for NOTIFICATION_HISTORY_URL_VISITED.
struct URLVisitedDetails : public HistoryDetails {
  URLVisitedDetails();
  virtual ~URLVisitedDetails();

  content::PageTransition transition;

  // The affected URLRow. The ID will be set to the value that is currently in
  // effect in the main history database.
  URLRow row;

  // A list of redirects leading up to the URL represented by this struct. If
  // we have the redirect chain A -> B -> C and this struct represents visiting
  // C, then redirects[0]=B and redirects[1]=A.  If there are no redirects,
  // this will be an empty vector.
  history::RedirectList redirects;

  base::Time visit_time;
};

// Details for NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED.
struct URLsModifiedDetails : public HistoryDetails {
  URLsModifiedDetails();
  virtual ~URLsModifiedDetails();

  // Lists the information for each of the URLs affected. The rows will have the
  // IDs that are currently in effect in the main history database.
  URLRows changed_urls;
};

// Details for NOTIFICATION_HISTORY_URLS_DELETED.
struct URLsDeletedDetails : public HistoryDetails {
  URLsDeletedDetails();
  virtual ~URLsDeletedDetails();

  // Set when all history was deleted. False means just a subset was deleted.
  bool all_history;

  // True if the data was expired due to old age. False if the data was deleted
  // in response to an explicit user action through the History UI.
  bool expired;

  // The URLRows of URLs deleted. This is valid only when |all_history| is false
  // indicating that a subset of history has been deleted. The rows will have
  // the IDs that had been in effect before the deletion in the main history
  // database.
  URLRows rows;

  // The list of deleted favicon urls. This is valid only when |all_history| is
  // false, indicating that a subset of history has been deleted.
  std::set<GURL> favicon_urls;
};

// Details for HISTORY_KEYWORD_SEARCH_TERM_UPDATED.
struct KeywordSearchUpdatedDetails : public HistoryDetails {
  KeywordSearchUpdatedDetails(const URLRow& url_row,
                              KeywordID keyword_id,
                              const base::string16& term);
  virtual ~KeywordSearchUpdatedDetails();

  // The affected URLRow. The ID will be set to the value that is currently in
  // effect in the main history database.
  URLRow url_row;
  KeywordID keyword_id;
  base::string16 term;
};

// Details for HISTORY_KEYWORD_SEARCH_TERM_DELETED.
struct KeywordSearchDeletedDetails : public HistoryDetails {
  explicit KeywordSearchDeletedDetails(URLID url_row_id);
  virtual ~KeywordSearchDeletedDetails();

  // The ID of the corresponding URLRow in the main history database.
  URLID url_row_id;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__
