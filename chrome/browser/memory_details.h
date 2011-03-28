// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_DETAILS_H_
#define CHROME_BROWSER_MEMORY_DETAILS_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "content/common/child_process_info.h"

// We collect data about each browser process.  A browser may
// have multiple processes (of course!).  Even IE has multiple
// processes these days.
struct ProcessMemoryInformation {
  ProcessMemoryInformation();
  ~ProcessMemoryInformation();

  // The process id.
  base::ProcessId pid;
  // The working set information.
  base::WorkingSetKBytes working_set;
  // The committed bytes.
  base::CommittedKBytes committed;
  // The process version
  string16 version;
  // The process product name.
  string16 product_name;
  // The number of processes which this memory represents.
  int num_processes;
  // A process is a diagnostics process if it is rendering about:memory.
  // Mark this specially so that it can avoid counting it in its own
  // results.
  bool is_diagnostics;
  // If this is a child process of Chrome, what type (i.e. plugin) it is.
  ChildProcessInfo::ProcessType type;
  // If this is a renderer process, what type it is.
  ChildProcessInfo::RendererProcessType renderer_type;
  // A collection of titles used, i.e. for a tab it'll show all the page titles.
  std::vector<string16> titles;
};

typedef std::vector<ProcessMemoryInformation> ProcessMemoryInformationList;

// Browser Process Information.
struct ProcessData {
  ProcessData();
  ProcessData(const ProcessData& rhs);
  ~ProcessData();
  ProcessData& operator=(const ProcessData& rhs);

  string16 name;
  string16 process_name;
  ProcessMemoryInformationList processes;
};

#if defined(OS_MACOSX)
class ProcessInfoSnapshot;
#endif

// MemoryDetails fetches memory details about current running browsers.
// Because this data can only be fetched asynchronously, callers use
// this class via a callback.
//
// Example usage:
//
//    class MyMemoryDetailConsumer : public MemoryDetails {
//
//      MyMemoryDetailConsumer() {
//        // Anything but |StartFetch()|.
//      }
//
//      // (Or just call |StartFetch()| explicitly if there's nothing else to
//      // do.)
//      void StartDoingStuff() {
//        StartFetch();  // Starts fetching details.
//        // Etc.
//      }
//
//      // Your other class stuff here
//
//      virtual void OnDetailsAvailable() {
//        // do work with memory info here
//      }
//    }
class MemoryDetails : public base::RefCountedThreadSafe<MemoryDetails> {
 public:
  // Constructor.
  MemoryDetails();

  // Access to the process detail information.  This data is only available
  // after OnDetailsAvailable() has been called.
  const std::vector<ProcessData>& processes() { return process_data_; }

  // Initiate updating the current memory details.  These are fetched
  // asynchronously because data must be collected from multiple threads.
  // OnDetailsAvailable will be called when this process is complete.
  void StartFetch();

  virtual void OnDetailsAvailable() {}

 protected:
  friend class base::RefCountedThreadSafe<MemoryDetails>;

  virtual ~MemoryDetails();

 private:
  // Collect child process information on the IO thread.  This is needed because
  // information about some child process types (i.e. plugins) can only be taken
  // on that thread.  The data will be used by about:memory.  When finished,
  // invokes back to the file thread to run the rest of the about:memory
  // functionality.
  void CollectChildInfoOnIOThread();

  // Collect current process information from the OS and store it
  // for processing.  If data has already been collected, clears old
  // data and re-collects the data.
  // Note - this function enumerates memory details from many processes
  // and is fairly expensive to run, hence it's run on the file thread.
  // The parameter holds information about processes from the IO thread.
  void CollectProcessData(const std::vector<ProcessMemoryInformation>&);

#if defined(OS_MACOSX)
  // A helper for |CollectProcessData()|, collecting data on the Chrome/Chromium
  // process with PID |pid|. The collected data is added to the state of the
  // object (in |process_data_|).
  void CollectProcessDataChrome(
      const std::vector<ProcessMemoryInformation>& child_info,
      base::ProcessId pid,
      const ProcessInfoSnapshot& process_info);
#endif

  // Collect child process information on the UI thread.  Information about
  // renderer processes is only available there.
  void CollectChildInfoOnUIThread();

  // Each time we take a memory sample, we do a little work to update
  // the global histograms for tracking memory usage.
  void UpdateHistograms();

  // Returns a pointer to the ProcessData structure for Chrome.
  ProcessData* ChromeBrowser();

  std::vector<ProcessData> process_data_;

  DISALLOW_COPY_AND_ASSIGN(MemoryDetails);
};

#endif  // CHROME_BROWSER_MEMORY_DETAILS_H_
