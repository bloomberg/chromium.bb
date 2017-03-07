// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the collection of a stability file to a protocol buffer.

#ifndef COMPONENTS_BROWSER_WATCHER_POSTMORTEM_REPORT_EXTRACTOR_H_
#define COMPONENTS_BROWSER_WATCHER_POSTMORTEM_REPORT_EXTRACTOR_H_

#include "base/files/file_path.h"
#include "components/browser_watcher/stability_report.pb.h"

namespace browser_watcher {

// DO NOT CHANGE VALUES. This is logged persistently in a histogram.
enum CollectionStatus {
  NONE = 0,
  SUCCESS = 1,  // Successfully registered a report with Crashpad.
  ANALYZER_CREATION_FAILED = 2,
  DEBUG_FILE_NO_DATA = 3,
  PREPARE_NEW_CRASH_REPORT_FAILED = 4,
  WRITE_TO_MINIDUMP_FAILED = 5,
  DEBUG_FILE_DELETION_FAILED = 6,
  FINISHED_WRITING_CRASH_REPORT_FAILED = 7,
  COLLECTION_STATUS_MAX = 8
};

// Extracts a stability report from a stability file.
// TODO(manzagop): have a function that takes a GlobalActivityAnalyzer instead
// and simplify testing.
CollectionStatus Extract(const base::FilePath& stability_file,
                         StabilityReport* report);

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_POSTMORTEM_REPORT_EXTRACTOR_H_
