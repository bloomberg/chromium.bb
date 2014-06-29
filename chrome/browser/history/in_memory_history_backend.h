// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The InMemoryHistoryBackend is a wrapper around the in-memory URL database.
// It maintains an in-memory cache of a subset of history that is required for
// low-latency operations, such as in-line autocomplete.
//
// The in-memory cache provides the following guarantees:
//  (1.) It will always contain URLRows that either have a |typed_count| > 0; or
//       that have a corresponding search term, in which case information about
//       the search term is also stored.
//  (2.) It will be an actual subset, i.e., it will contain verbatim data, and
//       will never contain more data that can be found in the main database.
//
// The InMemoryHistoryBackend is created on the history thread and passed to the
// main thread where operations can be completed synchronously. It listens for
// notifications from the "regular" history backend and keeps itself in sync.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_HISTORY_BACKEND_H_
#define CHROME_BROWSER_HISTORY_IN_MEMORY_HISTORY_BACKEND_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "components/history/core/browser/keyword_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class FilePath;
}

namespace history {

class InMemoryDatabase;
struct KeywordSearchUpdatedDetails;
struct KeywordSearchDeletedDetails;
class URLDatabase;
class URLRow;
struct URLsDeletedDetails;
struct URLsModifiedDetails;

class InMemoryHistoryBackend : public content::NotificationObserver {
 public:
  InMemoryHistoryBackend();
  virtual ~InMemoryHistoryBackend();

  // Initializes the backend from the history database pointed to by the
  // full path in |history_filename|.
  bool Init(const base::FilePath& history_filename);

  // Does initialization work when this object is attached to the history
  // system on the main thread. The argument is the profile with which the
  // attached history service is under.
  void AttachToHistoryService(Profile* profile);

  // Deletes all search terms for the specified keyword.
  void DeleteAllSearchTermsForKeyword(KeywordID keyword_id);

  // Returns the underlying database associated with this backend. The current
  // autocomplete code was written fro this, but it should probably be removed
  // so that it can deal directly with this object, rather than the DB.
  InMemoryDatabase* db() const {
    return db_.get();
  }

  // Notification callback.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteAll);

  // Handler for HISTORY_URL_VISITED and HISTORY_URLS_MODIFIED.
  void OnURLVisitedOrModified(const URLRow& url_row);

  // Handler for HISTORY_URLS_DELETED.
  void OnURLsDeleted(const URLsDeletedDetails& details);

  // Handler for HISTORY_KEYWORD_SEARCH_TERM_UPDATED.
  void OnKeywordSearchTermUpdated(const KeywordSearchUpdatedDetails& details);

  // Handler for HISTORY_KEYWORD_SEARCH_TERM_DELETED.
  void OnKeywordSearchTermDeleted(const KeywordSearchDeletedDetails& details);

  content::NotificationRegistrar registrar_;

  scoped_ptr<InMemoryDatabase> db_;

  // The profile that this object is attached. May be NULL before
  // initialization.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryHistoryBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_HISTORY_BACKEND_H_
