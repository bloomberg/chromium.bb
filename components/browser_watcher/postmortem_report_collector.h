// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Following an unclean shutdown, a stability report can be collected and
// submitted for upload to a reporter.

#ifndef COMPONENTS_BROWSER_WATCHER_POSTMORTEM_REPORT_COLLECTOR_H_
#define COMPONENTS_BROWSER_WATCHER_POSTMORTEM_REPORT_COLLECTOR_H_

#include <stdio.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/debug/activity_analyzer.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/browser_watcher/stability_report.pb.h"
#include "components/browser_watcher/stability_report_extractor.h"
#include "components/browser_watcher/system_session_analyzer_win.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace browser_watcher {

// Deletes stability files.
class PostmortemDeleter {
 public:
  PostmortemDeleter() = default;
  ~PostmortemDeleter() = default;

  void Process(const std::vector<base::FilePath>& stability_files);
};

// Handles postmortem report collection by establishing the set of stability
// files to collect, then for each file:
//   - extracting a report protocol buffer
//   - registering a crash report with the crash database
//   - writing a minidump file for the report
// TODO(manzagop): throttling, graceful handling of accumulating data.
class PostmortemReportCollector {
 public:
  PostmortemReportCollector(const std::string& product_name,
                            const std::string& version_number,
                            const std::string& channel_name,
                            crashpad::CrashReportDatabase* report_database,
                            SystemSessionAnalyzer* analyzer);
  ~PostmortemReportCollector();

  // Collects postmortem stability reports from |stability_files|. Reports are
  // then wrapped in Crashpad reports and registered with the crash database.
  void Process(const std::vector<base::FilePath>& stability_files);

  const std::string& product_name() const { return product_name_; }
  const std::string& version_number() const { return version_number_; }
  const std::string& channel_name() const { return channel_name_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(PostmortemReportCollectorTest,
                           GetDebugStateFilePaths);
  FRIEND_TEST_ALL_PREFIXES(PostmortemReportCollectorTest, CollectEmptyFile);
  FRIEND_TEST_ALL_PREFIXES(PostmortemReportCollectorTest, CollectRandomFile);
  FRIEND_TEST_ALL_PREFIXES(PostmortemReportCollectorCollectionTest,
                           CollectSuccess);
  FRIEND_TEST_ALL_PREFIXES(
      PostmortemReportCollectorCollectionFromGlobalTrackerTest,
      LogCollection);
  FRIEND_TEST_ALL_PREFIXES(
      PostmortemReportCollectorCollectionFromGlobalTrackerTest,
      ProcessUserDataCollection);
  FRIEND_TEST_ALL_PREFIXES(
      PostmortemReportCollectorCollectionFromGlobalTrackerTest,
      FieldTrialCollection);
  FRIEND_TEST_ALL_PREFIXES(
      PostmortemReportCollectorCollectionFromGlobalTrackerTest,
      ModuleCollection);
  FRIEND_TEST_ALL_PREFIXES(
      PostmortemReportCollectorCollectionFromGlobalTrackerTest,
      SystemStateTest);

  // Collects a stability file, generates a report and registers it with the
  // database.
  void CollectAndSubmitOneReport(const crashpad::UUID& client_id,
                                 const base::FilePath& file);

  virtual CollectionStatus CollectOneReport(
      const base::FilePath& stability_file,
      StabilityReport* report);

  void SetReporterDetails(StabilityReport* report) const;

  void RecordSystemShutdownState(StabilityReport* report) const;

  virtual bool WriteReportToMinidump(StabilityReport* report,
                                     const crashpad::UUID& client_id,
                                     const crashpad::UUID& report_id,
                                     base::PlatformFile minidump_file);

  std::string product_name_;
  std::string version_number_;
  std::string channel_name_;

  crashpad::CrashReportDatabase* report_database_;  // Not owned.
  SystemSessionAnalyzer* system_session_analyzer_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(PostmortemReportCollector);
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_POSTMORTEM_REPORT_COLLECTOR_H_
