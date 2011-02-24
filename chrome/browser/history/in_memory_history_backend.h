// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the history backend wrapper around the in-memory URL database. This
// object maintains an in-memory cache of the subset of history required to do
// in-line autocomplete.
//
// It is created on the history thread and passed to the main thread where
// operations can be completed synchronously. It listens for notifications
// from the "regular" history backend and keeps itself in sync.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_HISTORY_BACKEND_H_
#define CHROME_BROWSER_HISTORY_IN_MEMORY_HISTORY_BACKEND_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class FilePath;
class GURL;
class HistoryDatabase;
class Profile;

namespace history {

class InMemoryDatabase;
class InMemoryURLIndex;
struct KeywordSearchTermDetails;
class URLDatabase;
struct URLsDeletedDetails;
struct URLsModifiedDetails;

class InMemoryHistoryBackend : public NotificationObserver {
 public:
  InMemoryHistoryBackend();
  ~InMemoryHistoryBackend();

  // Initializes the backend from the history database pointed to by the
  // full path in |history_filename|. |history_dir| is the path to the
  // directory containing the history database and is also used
  // as the directory where the InMemoryURLIndex's cache is kept. |db| is
  // used for building the InMemoryURLIndex. |languages| gives the
  // preferred user languages with which URLs and page titles are
  // interpreted while decomposing into words and characters during indexing.
  bool Init(const FilePath& history_filename,
            const FilePath& history_dir,
            URLDatabase* db,
            const std::string& languages);

  // Does initialization work when this object is attached to the history
  // system on the main thread. The argument is the profile with which the
  // attached history service is under.
  void AttachToHistoryService(Profile* profile);

  // Returns the underlying database associated with this backend. The current
  // autocomplete code was written fro this, but it should probably be removed
  // so that it can deal directly with this object, rather than the DB.
  InMemoryDatabase* db() const {
    return db_.get();
  }

  // Notification callback.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Return the quick history index.
  history::InMemoryURLIndex* InMemoryIndex() const { return index_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteAll);

  // Handler for NOTIFY_HISTORY_TYPED_URLS_MODIFIED.
  void OnTypedURLsModified(const URLsModifiedDetails& details);

  // Handler for NOTIFY_HISTORY_URLS_DELETED.
  void OnURLsDeleted(const URLsDeletedDetails& details);

  // Handler for HISTORY_KEYWORD_SEARCH_TERM_UPDATED.
  void OnKeywordSearchTermUpdated(const KeywordSearchTermDetails& details);

  // Returns true if there is a keyword associated with the specified url.
  bool HasKeyword(const GURL& url);

  NotificationRegistrar registrar_;

  scoped_ptr<InMemoryDatabase> db_;

  // The profile that this object is attached. May be NULL before
  // initialization.
  Profile* profile_;

  // The index used for quick history lookups.
  scoped_ptr<history::InMemoryURLIndex> index_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryHistoryBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_HISTORY_BACKEND_H_
