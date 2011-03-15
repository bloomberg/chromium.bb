// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Structs that hold data used in broadcasting notifications.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__
#define CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__
#pragma once

#include <set>
#include <vector>

#include "googleurl/src/gurl.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/search_engines/template_url_id.h"

namespace history {

// Base class for history notifications. This needs only a virtual destructor
// so that the history service's broadcaster can delete it when the request
// is complete.
struct HistoryDetails {
 public:
  virtual ~HistoryDetails() {}
};

// Details for HISTORY_URL_VISITED.
struct URLVisitedDetails : public HistoryDetails {
  URLVisitedDetails();
  virtual ~URLVisitedDetails();

  PageTransition::Type transition;
  URLRow row;

  // A list of redirects leading up to the URL represented by this struct. If
  // we have the redirect chain A -> B -> C and this struct represents visiting
  // C, then redirects[0]=B and redirects[1]=A.  If there are no redirects,
  // this will be an empty vector.
  history::RedirectList redirects;
};

// Details for NOTIFY_HISTORY_TYPED_URLS_MODIFIED.
struct URLsModifiedDetails : public HistoryDetails {
  URLsModifiedDetails();
  virtual ~URLsModifiedDetails();

  // Lists the information for each of the URLs affected.
  std::vector<URLRow> changed_urls;
};

// Details for NOTIFY_HISTORY_URLS_DELETED.
struct URLsDeletedDetails : public HistoryDetails {
  URLsDeletedDetails();
  virtual ~URLsDeletedDetails();

  // Set when all history was deleted. False means just a subset was deleted.
  bool all_history;

  // The list of unique URLs affected. This is valid only when a subset of
  // history is deleted. When all of it is deleted, this will be empty, since
  // we do not bother to list all URLs.
  std::set<GURL> urls;
};

// Details for NOTIFY_URLS_STARRED.
struct URLsStarredDetails : public HistoryDetails {
  explicit URLsStarredDetails(bool being_starred);
  virtual ~URLsStarredDetails();

  // The new starred state of the list of URLs. True when they are being
  // starred, false when they are being unstarred.
  bool starred;

  // The list of URLs that are changing.
  std::set<GURL> changed_urls;
};

// Details for NOTIFY_FAVICON_CHANGED.
struct FaviconChangeDetails : public HistoryDetails {
  FaviconChangeDetails();
  virtual ~FaviconChangeDetails();

  std::set<GURL> urls;
};

// Details for HISTORY_KEYWORD_SEARCH_TERM_UPDATED.
struct KeywordSearchTermDetails : public HistoryDetails {
  KeywordSearchTermDetails();
  ~KeywordSearchTermDetails();

  GURL url;
  TemplateURLID keyword_id;
  string16 term;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_NOTIFICATIONS_H__
