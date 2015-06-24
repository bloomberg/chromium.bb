// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_

#include <string>

namespace base {
class FilePath;
}

namespace safe_browsing {

// Reporter exit codes.
const int kSwReporterCleanupNeeded = 0;
const int kSwReporterNothingFound = 2;
const int kSwReporterPostRebootCleanupNeeded = 4;
const int kSwReporterDelayedPostRebootCleanupNeeded = 15;

// Tries to run the sw_reporter component, and schedule the next try.
void RunSwReporter(const base::FilePath& exe_path, const std::string& version);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_
