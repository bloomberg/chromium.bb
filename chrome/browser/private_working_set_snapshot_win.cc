// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_working_set_snapshot.h"

#include <pdh.h>
#include <pdhmsg.h>

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "base/win/windows_version.h"

// Link pdh.lib (import library for pdh.dll) whenever this file is linked in.
#pragma comment(lib, "pdh.lib")

PrivateWorkingSetSnapshot::PrivateWorkingSetSnapshot() {}

PrivateWorkingSetSnapshot::~PrivateWorkingSetSnapshot() {}

void PrivateWorkingSetSnapshot::Initialize() {
  DCHECK(!initialized_);

  // Set this to true whether initialization succeeds or not. That is, only try
  // once.
  initialized_ = true;

  // The Pdh APIs are supported on Windows XP and above, but the "Working Set -
  // Private" counter that PrivateWorkingSetSnapshot depends on is not defined
  // until Windows Vista and is not reliable until Windows 7. Early-out to avoid
  // wasted effort. All queries will return zero and will have to use the
  // fallback calculations.
  if (base::win::GetVersion() <= base::win::VERSION_VISTA) {
    process_names_.clear();
    return;
  }

  // Pdh.dll has a format-string bug that causes crashes on Windows 8 and 8.1.
  // This was patched (around October 2014) but the broken versions are still on
  // a few machines. CreateFileVersionInfoForModule touches the disk so it
  // be used on the main thread. If this call gets moved off of the main thread
  // then pdh.dll version checking can be found in the history of
  // https://codereview.chromium.org/1269223005

  // Create a Pdh query
  PDH_HQUERY query_handle;
  if (PdhOpenQuery(NULL, NULL, &query_handle) != ERROR_SUCCESS) {
    process_names_.clear();
    return;
  }

  query_handle_.Set(query_handle);

  for (const auto& process_name : process_names_) {
    AddToMonitorList(process_name);
  }
  process_names_.clear();
}

void PrivateWorkingSetSnapshot::AddToMonitorList(
    const std::string& process_name) {
  if (!query_handle_.IsValid()) {
    // Save the name for later.
    if (!initialized_)
      process_names_.push_back(process_name);
    return;
  }

  // Create the magic strings that will return a list of process IDs and a list
  // of private working sets. The 'process_name' variable should be something
  // like "chrome". The '*' character indicates that we want records for all
  // processes whose names start with process_name - all chrome processes, but
  // also all 'chrome_editor.exe' processes or other matching names. The excess
  // information is unavoidable but harmless.
  std::string process_id_query = "\\Process(" + process_name + "*)\\ID Process";
  std::string private_ws_query =
      "\\Process(" + process_name + "*)\\Working Set - Private";

  // Add the two counters to the query.
  PdhCounterPair new_counters;
  if (PdhAddCounterA(query_handle_.Get(), process_id_query.c_str(), NULL,
                     &new_counters.process_id_handle) != ERROR_SUCCESS) {
    return;
  }

  // If adding the second counter fails then we should remove the first one.
  if (PdhAddCounterA(query_handle_.Get(), private_ws_query.c_str(), NULL,
                     &new_counters.private_ws_handle) != ERROR_SUCCESS) {
    PdhRemoveCounter(new_counters.process_id_handle);
  }

  // Record the pair of counter query handles so that we can query them later.
  counter_pairs_.push_back(new_counters);
}

void PrivateWorkingSetSnapshot::Sample() {
  // Make sure this is called once.
  if (!initialized_)
    Initialize();

  if (counter_pairs_.empty())
    return;

  // Destroy all previous data.
  records_.resize(0);
  // Record the requested data into PDH's internal buffers.
  if (PdhCollectQueryData(query_handle_.Get()) != ERROR_SUCCESS)
    return;

  for (auto& counter_pair : counter_pairs_) {
    // Find out how much space is required for the two counter arrays.
    // A return code of PDH_MORE_DATA indicates that we should call again with
    // the buffer size returned.
    DWORD buffer_size1 = 0;
    DWORD item_count1 = 0;
    // Process IDs should be retrieved as PDH_FMT_LONG
    if (PdhGetFormattedCounterArray(counter_pair.process_id_handle,
                                    PDH_FMT_LONG, &buffer_size1, &item_count1,
                                    nullptr) != PDH_MORE_DATA)
      continue;
    if (buffer_size1 == 0 || item_count1 == 0)
      continue;

    DWORD buffer_size2 = 0;
    DWORD item_count2 = 0;
    // Working sets should be retrieved as PDH_FMT_LARGE (LONGLONG)
    // Note that if this second call to PdhGetFormattedCounterArray with the
    // buffer size and count variables being zero is omitted then the PID and
    // working-set results are not reliably correlated.
    if (PdhGetFormattedCounterArray(counter_pair.private_ws_handle,
                                    PDH_FMT_LARGE, &buffer_size2, &item_count2,
                                    nullptr) != PDH_MORE_DATA)
      continue;

    // It is not clear whether Pdh guarantees that the two counters in the same
    // query will execute atomically - if they will see the same set of
    // processes. If they do not then the correspondence between "ID Process"
    // and "Working Set - Private" is lost and we have to discard these results.
    // In testing these values have always matched. If this check fails then
    // the old per-process memory calculations will be used instead.
    if (buffer_size1 != buffer_size2 || item_count1 != item_count2)
      continue;

    // Allocate enough space for the results of both queries.
    std::vector<char> buffer(buffer_size1 * 2);
    // Retrieve the process ID data.
    auto process_id_data =
        reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(&buffer[0]);
    if (PdhGetFormattedCounterArray(counter_pair.process_id_handle,
                                    PDH_FMT_LONG, &buffer_size1, &item_count1,
                                    process_id_data) != ERROR_SUCCESS)
      continue;
    // Retrieve the private working set data.
    auto private_ws_data =
        reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(&buffer[buffer_size1]);
    if (PdhGetFormattedCounterArray(counter_pair.private_ws_handle,
                                    PDH_FMT_LARGE, &buffer_size1, &item_count1,
                                    private_ws_data) != ERROR_SUCCESS)
      continue;

    // Make room for the new set of records.
    size_t start_offset = records_.size();
    records_.resize(start_offset + item_count1);

    for (DWORD i = 0; i < item_count1; ++i) {
      records_[start_offset + i].process_id =
          process_id_data[i].FmtValue.longValue;
      // Integer overflow can happen here if a 32-bit process is monitoring a
      // 64-bit process so we do a saturated_cast.
      records_[start_offset + i].private_ws =
          base::saturated_cast<size_t>(private_ws_data[i].FmtValue.largeValue);
    }
  }

  // The results will include all processes that match the passed in name,
  // regardless of whether they are spawned by the calling process.
  // The results must be sorted by process ID for efficient lookup.
  std::sort(records_.begin(), records_.end());
}

size_t PrivateWorkingSetSnapshot::GetPrivateWorkingSet(
    base::ProcessId process_id) const {
  // Do a binary search for the requested process ID and return the working set
  // if found.
  auto p = std::lower_bound(records_.begin(), records_.end(), process_id);
  if (p != records_.end() && p->process_id == process_id)
    return p->private_ws;

  return 0;
}
