// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_WORKING_SET_SNAPSHOT_H_
#define CHROME_BROWSER_PRIVATE_WORKING_SET_SNAPSHOT_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <pdh.h>

#include <vector>

#include "base/process/process_handle.h"
#include "base/win/scoped_handle.h"

namespace win {

// The traits class for PDH handles that can be closed via PdhCloseQuery() API.
struct PDHHandleTraits {
  typedef PDH_HQUERY Handle;
  static PDH_HQUERY NullHandle() { return nullptr; }
  static bool IsHandleValid(PDH_HQUERY handle) { return handle != nullptr; }
  static bool CloseHandle(PDH_HQUERY handle) {
    return (PdhCloseQuery(handle) == ERROR_SUCCESS);
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PDHHandleTraits);
};

// DummyVerifierTraits can be used because PDH_HQUERY is just a typedef for
// HANDLE. However HandleTraits cannot be used because PdhCloseQuery must be
// called rather than CloseHandle to dispose of the resources.
using ScopedPDH =
    base::win::GenericScopedHandle<PDHHandleTraits,
                                   base::win::DummyVerifierTraits>;

}  // namespace win

// This class can be used to do bulk collection of private working sets. This
// exists because on Windows it is much faster to collect a group of private
// working sets all at once using PdhOpenQuery than to calculate the private
// working sets for each process individually.
class PrivateWorkingSetSnapshot {
 public:
  PrivateWorkingSetSnapshot();
  ~PrivateWorkingSetSnapshot();

  // Add a process name that this object should monitor, such as "chrome". All
  // processes whose name starts with this string will be monitored.
  void AddToMonitorList(const std::string& process_name);

  // Query the system for working-set information for all monitored processes
  // and update the results cache. This function may take a few ms to run.
  // The time it takes seems to be independent of the number of processes it
  // retrieves data for. This makes it faster than using QueryWorkingSet as soon
  // as the process count exceeds two or three.
  void Sample();

  // Ask for the working set for a specific process, from the most recent call
  // to Sample. If no data is available then zero will be returned. The result
  // is in bytes.
  size_t GetPrivateWorkingSet(base::ProcessId process_id) const;

 private:
  // Initialize the PDH |query_handle_| and process any pending AddToMonitorList
  // calls.
  void Initialize();

  // This holds a pair of Pdh counters to queries for the process ID and private
  // working set for a particular process name being monitored. The results from
  // the two queries can be matched up so that we can associate a private
  // working set with a process ID.
  struct PdhCounterPair {
    // These are bound to query_handle_ and will be freed when it is closed.
    // The handle to the 'counter' that retrieves process IDs.
    PDH_HCOUNTER process_id_handle = nullptr;
    // The handle to the 'counter' that retrieves private working sets.
    PDH_HCOUNTER private_ws_handle = nullptr;
  };

  // Struct for storing a process ID and associated private working set.
  struct PidAndPrivateWorkingSet {
    base::ProcessId process_id;
    size_t private_ws;
    // Comparison function for sorting by process ID.
    bool operator<(const PidAndPrivateWorkingSet& other) const {
      // private_ws is intentionally *not* part of the comparison because it is
      // the payload and process_id is the key.
      return process_id < other.process_id;
    }
    // Comparison function for searching by process ID.
    bool operator<(const base::ProcessId other_process_id) const {
      return process_id < other_process_id;
    }
  };

  // True if Initialize() has been called, whether it succeeded or not.
  bool initialized_ = false;

  // The handle to the query object.
  win::ScopedPDH query_handle_;

  // A vector of process names to monitor when initialization occurs.
  std::vector<std::string> process_names_;

  // A PdhCounterPair for each successful AddToMonitorList call.
  std::vector<PdhCounterPair> counter_pairs_;

  // After each call to Sample this will hold the results, sorted by process id.
  std::vector<PidAndPrivateWorkingSet> records_;

  DISALLOW_COPY_AND_ASSIGN(PrivateWorkingSetSnapshot);
};

#endif  // defined(OS_WIN)

#endif  // CHROME_BROWSER_PRIVATE_WORKING_SET_SNAPSHOT_H_
