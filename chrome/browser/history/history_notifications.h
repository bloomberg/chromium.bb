// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Structs that hold data used in broadcasting notifications.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__
#define CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__

#include <set>

#include "chrome/browser/history/history_details.h"
#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

namespace history {

// Details for NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED.
struct URLsModifiedDetails : public HistoryDetails {
  URLsModifiedDetails();
  ~URLsModifiedDetails() override;

  // Lists the information for each of the URLs affected. The rows will have the
  // IDs that are currently in effect in the main history database.
  URLRows changed_urls;
};

// Details for NOTIFICATION_HISTORY_URLS_DELETED.
struct URLsDeletedDetails : public HistoryDetails {
  URLsDeletedDetails();
  ~URLsDeletedDetails() override;

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

// Details for HISTORY_KEYWORD_SEARCH_TERM_DELETED.
struct KeywordSearchDeletedDetails : public HistoryDetails {
  explicit KeywordSearchDeletedDetails(URLID url_row_id);
  ~KeywordSearchDeletedDetails() override;

  // The ID of the corresponding URLRow in the main history database.
  URLID url_row_id;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__
