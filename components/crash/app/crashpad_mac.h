// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_APP_CRASHPAD_MAC_H_
#define COMPONENTS_CRASH_APP_CRASHPAD_MAC_H_

#include <time.h>

#include <string>
#include <vector>

namespace crash_reporter {

// Initializes Crashpad in a way that is appropriate for process_type. If
// process_type is empty, initializes Crashpad for the browser process, which
// starts crashpad_handler and sets it as the exception handler. Other process
// types inherit this exception handler from the browser, but still need to
// perform additional initialization.
void InitializeCrashpad(const std::string& process_type);

// Enables or disables crash report upload. This is a property of the Crashpad
// database. In a newly-created database, uploads will be disabled. This
// function only has an effect when called in the browser process. Its effect is
// immediate and applies to all other process types, including processes that
// are already running.
void SetUploadsEnabled(bool enabled);

// Determines whether uploads are enabled or disabled. This information is only
// available in the browser process.
bool GetUploadsEnabled();

struct UploadedReport {
  std::string local_id;
  std::string remote_id;
  time_t creation_time;
};

// Obtains a list of reports uploaded to the collection server. This function
// only operates when called in the browser process. All reports in the Crashpad
// database that have been successfully uploaded will be included in this list.
// The list will be sorted in descending order by report creation time (newest
// reports first).
//
// TODO(mark): The about:crashes UI expects to show only uploaded reports. If it
// is ever enhanced to work well with un-uploaded reports, those should be
// returned as well. Un-uploaded reports may have a pending upload, may have
// experienced upload failure, or may have been collected while uploads were
// disabled.
void GetUploadedReports(std::vector<UploadedReport>* uploaded_reports);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_APP_CRASHPAD_MAC_H_
