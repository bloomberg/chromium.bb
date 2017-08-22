// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crash_export_thunks.h"

#include <algorithm>
#include <type_traits>

#include "components/crash/content/app/crashpad.h"

void RequestSingleCrashUploadThunk(const char* local_id) {
  crash_reporter::RequestSingleCrashUploadImpl(local_id);
}

size_t GetCrashReportsImpl(crash_reporter::Report* reports,
                           size_t reports_size) {
  static_assert(std::is_pod<crash_reporter::Report>::value,
                "crash_reporter::Report must be POD");
  // Since this could be called across module boundaries, retrieve the full
  // list of reports into this vector, and then manually copy however much fits
  // into the caller's copy.
  std::vector<crash_reporter::Report> crash_reports;

  // The crash_reporter::GetReports function thunks here, here is delegation to
  // the actual implementation.
  crash_reporter::GetReportsImpl(&crash_reports);

  size_t to_copy = std::min(reports_size, crash_reports.size());
  for (size_t i = 0; i < to_copy; ++i)
    reports[i] = crash_reports[i];

  return crash_reports.size();
}
