// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_DATABASE_H_
#define CHROME_BROWSER_HISTORY_IN_MEMORY_DATABASE_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/history/url_database.h"
#include "sql/connection.h"

class FilePath;

namespace history {

// Class used for a fast in-memory cache of typed URLs. Used for inline
// autocomplete since it is fast enough to be called synchronously as the user
// is typing.
class InMemoryDatabase : public URLDatabase {
 public:
  InMemoryDatabase();
  virtual ~InMemoryDatabase();

  // Creates an empty in-memory database.
  bool InitFromScratch();

  // Initializes the database by directly slurping the data from the given
  // file. Conceptually, the InMemoryHistoryBackend should do the populating
  // after this object does some common initialization, but that would be
  // much slower.
  bool InitFromDisk(const FilePath& history_name);

 protected:
  // Implemented for URLDatabase.
  virtual sql::Connection& GetDB() OVERRIDE;

 private:
  // Initializes the database connection, this is the shared code between
  // InitFromScratch() and InitFromDisk() above. Returns true on success.
  bool InitDB();

  sql::Connection db_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_DATABASE_H_
