// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_metrics_provider_win.h"

#include <limits>
#include <vector>

#include "base/metrics/sparse_histogram.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace browser_watcher {

namespace {

void CompileAsserts() {
  // Process ID APIs on Windows talk in DWORDs, whereas for string formatting
  // and parsing, this code uses int. In practice there are no process IDs with
  // the high bit set on Windows, so there's no danger of overflow if this is
  // done consistently.
  static_assert(sizeof(DWORD) == sizeof(int),
                "process ids are expected to be no larger than int");
}

// This function does soft matching on the PID recorded in the key only.
// Due to PID reuse, the possibility exists that the process that's now live
// with the given PID is not the same process the data was recorded for.
// This doesn't matter for the purpose, as eventually the data will be
// scavenged and reported.
bool IsDeadProcess(base::StringPiece16 key_or_value_name) {
  // Truncate the input string to the first occurrence of '-', if one exists.
  size_t num_end = key_or_value_name.find(L'-');
  if (num_end != base::StringPiece16::npos)
    key_or_value_name = key_or_value_name.substr(0, num_end);

  // Convert to the numeric PID.
  int pid = 0;
  if (!base::StringToInt(key_or_value_name, &pid) || pid == 0)
    return true;

  // This is a very inexpensive check for the common case of our own PID.
  if (static_cast<base::ProcessId>(pid) == base::GetCurrentProcId())
    return false;

  // The process is not our own - see whether a process with this PID exists.
  // This is more expensive than the above check, but should also be very rare,
  // as this only happens more than once for a given PID if a user is running
  // multiple Chrome instances concurrently.
  base::Process process =
      base::Process::Open(static_cast<base::ProcessId>(pid));
  if (process.IsValid()) {
    // The fact that it was possible to open the process says it's live.
    return false;
  }

  return true;
}

void RecordExitCodes(const base::string16& registry_path) {
  base::win::RegKey regkey(HKEY_CURRENT_USER,
                           registry_path.c_str(),
                           KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (!regkey.Valid())
    return;

  size_t num = regkey.GetValueCount();
  if (num == 0)
    return;
  std::vector<base::string16> to_delete;

  // Record the exit codes in a sparse stability histogram, as the range of
  // values used to report failures is large.
  base::HistogramBase* exit_code_histogram =
      base::SparseHistogram::FactoryGet(
          WatcherMetricsProviderWin::kBrowserExitCodeHistogramName,
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

void ReadSingleExitFunnel(
    base::win::RegKey* parent_key, const base::char16* name,
    std::vector<std::pair<base::string16, int64>>* events_out) {
  DCHECK(parent_key);
  DCHECK(name);
  DCHECK(events_out);

  base::win::RegKey regkey(parent_key->Handle(), name, KEY_READ | KEY_WRITE);
  if (!regkey.Valid())
    return;

  // Exit early if no work to do.
  size_t num = regkey.GetValueCount();
  if (num == 0)
    return;

  // Enumerate the recorded events for this process for processing.
  std::vector<std::pair<base::string16, int64>> events;
  for (size_t i = 0; i < num; ++i) {
    base::string16 event_name;
    LONG res = regkey.GetValueNameAt(static_cast<int>(i), &event_name);
    if (res == ERROR_SUCCESS) {
      int64 event_time = 0;
      res = regkey.ReadInt64(event_name.c_str(), &event_time);
      if (res == ERROR_SUCCESS)
        events.push_back(std::make_pair(event_name, event_time));
    }
  }

  // Attempt to delete the values before reporting anything.
  // Exit if this fails to make sure there is no double-reporting on e.g.
  // permission problems or other corruption.
  for (size_t i = 0; i < events.size(); ++i) {
    const base::string16& event_name = events[i].first;
    LONG res = regkey.DeleteValue(event_name.c_str());
    if (res != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to delete value " << event_name;
      return;
    }
  }

  events_out->swap(events);
}

void MaybeRecordSingleExitFunnel(base::win::RegKey* parent_key,
                                 const base::char16* name,
                                 bool report) {
  std::vector<std::pair<base::string16, int64>> events;
  ReadSingleExitFunnel(parent_key, name, &events);
  if (!report)
    return;

  // Find the earliest event time.
  int64 min_time = std::numeric_limits<int64>::max();
  for (size_t i = 0; i < events.size(); ++i)
    min_time = std::min(min_time, events[i].second);

  // Record the exit funnel event times in a sparse stability histogram.
  for (size_t i = 0; i < events.size(); ++i) {
    std::string histogram_name(
        WatcherMetricsProviderWin::kExitFunnelHistogramPrefix);
    histogram_name.append(base::WideToUTF8(events[i].first));
    base::TimeDelta event_time =
        base::Time::FromInternalValue(events[i].second) -
            base::Time::FromInternalValue(min_time);
    base::HistogramBase* histogram =
        base::SparseHistogram::FactoryGet(
            histogram_name.c_str(),
            base::HistogramBase::kUmaStabilityHistogramFlag);

    // Record the time rounded up to the nearest millisecond.
    histogram->Add(event_time.InMillisecondsRoundedUp());
  }
}

void MaybeRecordExitFunnels(const base::string16& registry_path, bool report) {
  base::win::RegistryKeyIterator it(HKEY_CURRENT_USER, registry_path.c_str());
  if (!it.Valid())
    return;

  // Exit early if no work to do.
  if (it.SubkeyCount() == 0)
    return;

  // Open the key we use for deletion preemptively to prevent reporting
  // multiple times on permission problems.
  base::win::RegKey key(HKEY_CURRENT_USER,
                        registry_path.c_str(),
                        KEY_QUERY_VALUE);
  if (!key.Valid()) {
    LOG(ERROR) << "Failed to open " << registry_path << " for writing.";
    return;
  }

  std::vector<base::string16> to_delete;
  for (; it.Valid(); ++it) {
    // Defer reporting on still-live processes.
    if (IsDeadProcess(it.Name())) {
      MaybeRecordSingleExitFunnel(&key, it.Name(), report);
      to_delete.push_back(it.Name());
    }
  }

  for (size_t i = 0; i < to_delete.size(); ++i) {
    LONG res = key.DeleteEmptyKey(to_delete[i].c_str());
    if (res != ERROR_SUCCESS)
      LOG(ERROR) << "Failed to delete key " << to_delete[i];
  }
}

}  // namespace

const char WatcherMetricsProviderWin::kBrowserExitCodeHistogramName[] =
    "Stability.BrowserExitCodes";
const char WatcherMetricsProviderWin::kExitFunnelHistogramPrefix[] =
    "Stability.ExitFunnel.";

WatcherMetricsProviderWin::WatcherMetricsProviderWin(
    const base::char16* registry_path, bool report_exit_funnels) :
        registry_path_(registry_path),
        report_exit_funnels_(report_exit_funnels) {
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
  RecordExitCodes(registry_path_);
  MaybeRecordExitFunnels(registry_path_, report_exit_funnels_);
}

}  // namespace browser_watcher
