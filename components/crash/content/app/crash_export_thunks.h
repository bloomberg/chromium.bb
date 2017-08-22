// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_APP_CRASH_EXPORT_THUNKS_H_
#define COMPONENTS_CRASH_CONTENT_APP_CRASH_EXPORT_THUNKS_H_

#include "components/crash/content/app/crashpad.h"

#include <stddef.h>

extern "C" {

// TODO(siggi): Rename these functions to something descriptive and unique,
//   once all the thunks have been converted to import binding.

// This function may be invoked across module boundaries to request a single
// crash report upload. See CrashUploadListCrashpad.
void RequestSingleCrashUploadImpl(const char* local_id);

// This function may be invoked across module boundaries to retrieve the crash
// list. It copies up to |report_count| reports into |reports| and returns the
// number of reports available. If the return value is less than or equal to
// |reports_size|, then |reports| contains all the available reports.
size_t GetCrashReportsImpl(crash_reporter::Report* reports,
                           size_t reports_size);

}  // extern "C"

#endif  // COMPONENTS_CRASH_CONTENT_APP_CRASH_EXPORT_THUNKS_H_
