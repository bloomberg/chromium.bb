// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_

#include <vector>

#include "components/history/core/browser/history_types.h"
#include "ui/base/page_transition_types.h"

namespace base {
class Time;
}

namespace typed_urls_helper {

// Gets the typed URLs from a specific sync profile.
history::URLRows GetTypedUrlsFromClient(int index);

// Gets a specific url from a specific sync profile. Returns false if the URL
// was not found in the history DB.
bool GetUrlFromClient(int index, const GURL& url, history::URLRow* row);

// Gets the visits for a URL from a specific sync profile.
history::VisitVector GetVisitsFromClient(int index, history::URLID id);

// Removes the passed |visits| from a specific sync profile.
void RemoveVisitsFromClient(int index, const history::VisitVector& visits);

// Adds a URL to the history DB for a specific sync profile (just registers a
// new visit if the URL already exists) using a TYPED PageTransition.
void AddUrlToHistory(int index, const GURL& url);

// Adds a URL to the history DB for a specific sync profile (just registers a
// new visit if the URL already exists), using the passed PageTransition.
void AddUrlToHistoryWithTransition(int index,
                                   const GURL& url,
                                   ui::PageTransition transition,
                                   history::VisitSource source);

// Adds a URL to the history DB for a specific sync profile (just registers a
// new visit if the URL already exists), using the passed PageTransition and
// timestamp.
void AddUrlToHistoryWithTimestamp(int index,
                                  const GURL& url,
                                  ui::PageTransition transition,
                                  history::VisitSource source,
                                  const base::Time& timestamp);

// Deletes a URL from the history DB for a specific sync profile.
void DeleteUrlFromHistory(int index, const GURL& url);

// Deletes a list of URLs from the history DB for a specific sync
// profile.
void DeleteUrlsFromHistory(int index, const std::vector<GURL>& urls);

// Returns true if all clients match the verifier profile.
bool CheckAllProfilesHaveSameURLsAsVerifier();

// Returns true if all clients match the verifier profile and if the
// verification doesn't time out.
bool AwaitCheckAllProfilesHaveSameURLsAsVerifier();

// Checks that the two vectors contain the same set of URLRows (possibly in
// a different order).
bool CheckURLRowVectorsAreEqual(const history::URLRows& left,
                                const history::URLRows& right);

// Checks that the passed URLRows are equivalent.
bool CheckURLRowsAreEqual(const history::URLRow& left,
                          const history::URLRow& right);

// Returns true if two sets of visits are equivalent.
bool AreVisitsEqual(const history::VisitVector& visit1,
                    const history::VisitVector& visit2);

// Returns true if there are no duplicate visit times.
bool AreVisitsUnique(const history::VisitVector& visits);

// Returns a unique timestamp to use when generating page visits
// (HistoryService does not like having identical timestamps and will modify
// the timestamps behind the scenes if it encounters them, which leads to
// spurious test failures when the resulting timestamps aren't what we
// expect).
base::Time GetTimestamp();

}  // namespace typed_urls_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_TYPED_URLS_HELPER_H_
