// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_SERVICE_OBSERVER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_SERVICE_OBSERVER_H_

#include "base/macros.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/keyword_id.h"

namespace history {

class HistoryService;

class HistoryServiceObserver {
 public:
  HistoryServiceObserver() {}
  virtual ~HistoryServiceObserver() {}

  // Called when user visits an URL.
  //
  // The |row| ID will be set to the value that is currently in effect in the
  // main history database. |redirects| is the list of redirects leading up to
  // the URL. If we have a redirect chain A -> B -> C and user is visiting C,
  // then |redirects[0]=B| and |redirects[1]=A|. If there are no redirects,
  // |redirects| is an empty vector.
  virtual void OnURLVisited(HistoryService* history_service,
                            ui::PageTransition transition,
                            const URLRow& row,
                            const RedirectList& redirects,
                            base::Time visit_time) {}

  // Called when a URL has been added or modified.
  //
  // |changed_urls| lists the information for each of the URLs affected. The
  // rows will have the IDs that are currently in effect in the main history
  // database.
  virtual void OnURLsModified(HistoryService* history_service,
                              const URLRows& changed_urls) {}

  // Called when one or more of URLs are deleted.
  //
  // |all_history| is set to true, if all the URLs are deleted.
  //               When set to true, |deleted_rows| and |favicon_urls| are
  //               undefined.
  // |expired| is set to true, if the URL deletion is due to expiration.
  // |deleted_rows| list of the deleted URLs.
  // |favicon_urls| list of favicon URLs that correspond to the deleted URLs.
  // DEPRECATED, use OnURLsDeleted() with |time_range| parameter.
  // TODO(dullweber): Migrate observers to new OnURLsDeleted() method.
  virtual void OnURLsDeleted(HistoryService* history_service,
                             bool all_history,
                             bool expired,
                             const URLRows& deleted_rows,
                             const std::set<GURL>& favicon_urls) {}

  // Called when one or more of URLs are deleted.
  //
  // |time_range| If time_range.IsValid() is true, all URLs between
  //              time_range.begin() and time_range.end() have been removed.
  //              If time_range.IsAllTime() is true, all URLs are deleted and
  //              |deleted_rows| and |favicon_urls| are undefined.
  // |expired| is set to true, if the URL deletion is due to expiration.
  // |deleted_rows| list of the deleted URLs.
  // |favicon_urls| list of favicon URLs that correspond to the deleted URLs.
  virtual void OnURLsDeleted(HistoryService* history_service,
                             const DeletionTimeRange& time_range,
                             bool expired,
                             const URLRows& deleted_rows,
                             const std::set<GURL>& favicon_urls) {}

  // Is called to notify when |history_service| has finished loading.
  virtual void OnHistoryServiceLoaded(HistoryService* history_service) {}

  // Is called to notify when |history_service| is being deleted.
  virtual void HistoryServiceBeingDeleted(HistoryService* history_service) {}

  // Sent when a keyword search term is updated.
  //
  // |row| contains the URL information for search |term|.
  // |keyword_id| associated with a URL and search term.
  virtual void OnKeywordSearchTermUpdated(HistoryService* history_service,
                                          const URLRow& row,
                                          KeywordID keyword_id,
                                          const base::string16& term) {}

  // Sent when a keyword search term is deleted.
  // |url_id| is the id of the url row.
  virtual void OnKeywordSearchTermDeleted(HistoryService* history_service,
                                          URLID url_id) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryServiceObserver);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_SERVICE_OBSERVER_H_
