// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_DELTA_FILE_BACKEND_LEVELDB_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_DELTA_FILE_BACKEND_LEVELDB_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"

class GURL;

namespace leveldb {
class DB;
}

namespace history_report {

class DeltaFileEntryWithData;

// Backend for delta file.
class DeltaFileBackend {
 public:
  explicit DeltaFileBackend(const base::FilePath& dir);
  ~DeltaFileBackend();

  // Adds new addition entry to delta file
  void PageAdded(const GURL& url);
  // Adds new deletion entry to delta file
  void PageDeleted(const GURL& url);
  // Removes all delta file entries with
  // sequence number <= min(|lower_bound|, max sequence number - 1).
  // Returns max sequence number in delta file.
  int64 Trim(int64 lower_bound);
  // Recreates delta file using given |urls|.
  bool Recreate(const std::vector<std::string>& urls);
  // Provides up to |limit| delta file entries with
  // sequence number > |last_seq_no|.
  scoped_ptr<std::vector<DeltaFileEntryWithData> > Query(int64 last_seq_no,
                                                         int32 limit);
  // Removes all entries from delta file
  void Clear();

  // Dumps internal state to string. For debuging.
  std::string Dump();

 private:
  // Starts delta file backend.
  bool Init();

  bool EnsureInitialized();

  class DigitsComparator;

  base::FilePath path_;
  scoped_ptr<leveldb::DB> db_;
  scoped_ptr<DigitsComparator> leveldb_cmp_;

  DISALLOW_COPY_AND_ASSIGN(DeltaFileBackend);
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_DELTA_FILE_BACKEND_LEVELDB_H_
