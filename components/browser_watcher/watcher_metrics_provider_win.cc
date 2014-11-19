// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_metrics_provider_win.h"

#include <vector>

#include "base/metrics/sparse_histogram.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/win/registry.h"

namespace browser_watcher {

namespace {

void CompileAsserts() {
  // Process ID APIs on Windows talk in DWORDs, whereas for string formatting
  // and parsing, this code uses int. In practice there are no process IDs with
  // the high bit set on Windows, so there's no danger of overflow if this is
  // done consistently.
  COMPILE_ASSERT(sizeof(DWORD) == sizeof(int),
                 process_ids_have_outgrown_an_int);
}

// This function does soft matching on the PID recorded in the key only.
// Due to PID reuse, the possibility exists that the process that's now live
// with the given PID is not the same process the data was recorded for.
// This doesn't matter for the purpose, as eventually the data will be
// scavenged and reported.
bool IsDeadProcess(base::StringPiece16 key_name) {
  // Truncate the input string to the first occurrence of '-', if one exists.
  size_t num_end = key_name.find(L'-');
  if (num_end != base::StringPiece16::npos)
    key_name = key_name.substr(0, num_end);

  // Convert to the numeric PID.
  int pid = 0;
  if (!base::StringToInt(key_name, &pid) || pid == 0)
    return true;

  // This is a very inexpensive check for the common case of our own PID.
  if (static_cast<base::ProcessId>(pid) == base::GetCurrentProcId())
    return false;

  // The process is not our own - see whether a process with this PID exists.
  // This is more expensive than the above check, but should also be very rare,
  // as this only happens more than once for a given PID if a user is running
  // multiple Chrome instances concurrently.
  base::ProcessHandle process = base::kNullProcessHandle;
  if (base::OpenProcessHandle(static_cast<base::ProcessId>(pid), &process)) {
    base::CloseProcessHandle(process);

    // The fact that it was possible to open the process says it's live.
    return false;
  }

  return true;
}

}  // namespace

const char WatcherMetricsProviderWin::kBrowserExitCodeHistogramName[] =
    "Stability.BrowserExitCodes";

WatcherMetricsProviderWin::WatcherMetricsProviderWin(
    const base::char16* registry_path) : registry_path_(registry_path) {
}

WatcherMetricsProviderWin::~WatcherMetricsProviderWin() {
}

void WatcherMetricsProviderWin::ProvideStabilityMetrics(
    metrics::SystemProfileProto* /* system_profile_proto */) {
  // Note that if there are multiple instances of Chrome running in the same
  // user account, there's a small race that will double-report the exit codes
  // from both/multiple instances. This ought to be vanishingly rare and will
  // only manifest as low-level "random" noise. To work around this it would be
  // necessary to implement some form of global locking, which is not worth it
  // here.
  base::win::RegKey regkey(HKEY_CURRENT_USER,
                           registry_path_.c_str(),
                           KEY_QUERY_VALUE | KEY_SET_VALUE);

  size_t num = regkey.GetValueCount();
  if (num != 0) {
    std::vector<base::string16> to_delete;

    // Record the exit codes in a sparse stability histogram, as the range of
    // values used to report failures is large.
    base::HistogramBase* exit_code_histogram =
        base::SparseHistogram::FactoryGet(kBrowserExitCodeHistogramName,
            base::HistogramBase::kUmaStabilityHistogramFlag);

    for (size_t i = 0; i < num; ++i) {
      base::string16 name;
      if (regkey.GetValueNameAt(static_cast<int>(i), &name) == ERROR_SUCCESS) {
        DWORD exit_code = 0;
        if (regkey.ReadValueDW(name.c_str(), &exit_code) == ERROR_SUCCESS) {
          // Do not report exit codes for processes that are still live,
          // notably for our own process.
          if (exit_code != STILL_ACTIVE || IsDeadProcess(name)) {
            to_delete.push_back(name);
            exit_code_histogram->Add(exit_code);
          }
        }
      }
    }

    // Delete the values reported above.
    for (size_t i = 0; i < to_delete.size(); ++i)
      regkey.DeleteValue(to_delete[i].c_str());
  }
}

}  // namespace browser_watcher
