// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORTS_BUFFER_BACKEND_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORTS_BUFFER_BACKEND_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class FilePath;
}  // namespace base

namespace leveldb {
class DB;
}  // namespace leveldb

namespace history_report {

class UsageReport;

// Stores usage reports which will be sent for history reporting in batches.
class UsageReportsBufferBackend {
 public:
  explicit UsageReportsBufferBackend(const base::FilePath& dir);

  ~UsageReportsBufferBackend();

  // Creates and initializes the internal data structures.
  bool Init();

  void AddVisit(const std::string& id, int64 timestamp_ms, bool typed_visit);

  // Returns a set of up to |amount| usage reports.
  scoped_ptr<std::vector<UsageReport> > GetUsageReportsBatch(int amount);

  void Remove(const std::vector<std::string>& reports);

  // Clears the buffer by removing all its usage reports.
  void Clear();

  // Dumps internal state to string. For debuging.
  std::string Dump();

 private:
  // NULL until Init method is called.
  scoped_ptr<leveldb::DB> db_;
  base::FilePath db_file_name_;

  DISALLOW_COPY_AND_ASSIGN(UsageReportsBufferBackend);
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORTS_BUFFER_BACKEND_H_
