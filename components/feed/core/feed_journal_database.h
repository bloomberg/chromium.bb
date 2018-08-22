// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_JOURNAL_DATABASE_H_
#define COMPONENTS_FEED_CORE_FEED_JOURNAL_DATABASE_H_

#include "base/files/file_path.h"
#include "base/macros.h"

namespace feed {

// FeedJournalDatabase is leveldb backend store for Feed's journal storage data.
// Feed's journal data are key-value pairs.
class FeedJournalDatabase {
 public:
  // Initializes the database with |database_folder|.
  explicit FeedJournalDatabase(const base::FilePath& database_folder);
  ~FeedJournalDatabase();

  DISALLOW_COPY_AND_ASSIGN(FeedJournalDatabase);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_JOURNAL_DATABASE_H_
