// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_SAMPLING_SHARED_SAMPLER_H_
#define CHROME_BROWSER_TASK_MANAGER_SAMPLING_SHARED_SAMPLER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace task_manager {

struct ProcessDataSnapshot;

// Defines sampler that will calculate resources for all processes all at once,
// on the worker thread. Created by TaskManagerImpl on the UI thread, but used
// mainly on a blocking pool thread.
//
// This exists because on Windows it is much faster to collect a group of
// process metrics for all processes all at once using NtQuerySystemInformation
// than to query the same data for for each process individually and because
// some types like Idle Wakeups can only be collected this way.
class SharedSampler : public base::RefCountedThreadSafe<SharedSampler> {
 public:
  explicit SharedSampler(
      const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner);

  // Below are the types of callbacks that are invoked on the UI thread
  // when the refresh is done on the worker thread.
  // These callbacks are passed via RegisterCallbacks.
  using OnIdleWakeupsCallback = base::Callback<void(int)>;
  using OnPhysicalMemoryCallback = base::Callback<void(int64_t)>;
  using OnStartTimeCallback = base::Callback<void(base::Time)>;
  using OnCpuTimeCallback = base::Callback<void(base::TimeDelta)>;

  // Returns a combination of refresh flags supported by the shared sampler.
  int64_t GetSupportedFlags() const;

  // Registers task group specific callbacks.
  void RegisterCallbacks(base::ProcessId process_id,
                         const OnIdleWakeupsCallback& on_idle_wakeups,
                         const OnPhysicalMemoryCallback& on_physical_memory,
                         const OnStartTimeCallback& on_start_time,
                         const OnCpuTimeCallback& on_cpu_time);

  // Unregisters task group specific callbacks.
  void UnregisterCallbacks(base::ProcessId process_id);

  // Refreshes the expensive process' stats (for now only idle wakeups per
  // second) on the worker thread.
  void Refresh(base::ProcessId process_id, int64_t refresh_flags);

#if defined(OS_WIN)
  // Specifies a function to use in place of NtQuerySystemInformation.
  typedef int (*QuerySystemInformationForTest)(unsigned char* buffer,
                                               int buffer_size);
  static void SetQuerySystemInformationForTest(
      QuerySystemInformationForTest query_system_information);
#endif  // defined(OS_WIN)

 private:
  friend class base::RefCountedThreadSafe<SharedSampler>;
  ~SharedSampler();

#if defined(OS_WIN)
  // The UI-thread callbacks in TaskGroup registered with RegisterCallbacks and
  // to be called when refresh on the worker thread is done.
  struct Callbacks {
    Callbacks();
    Callbacks(Callbacks&& other);
    ~Callbacks();

    OnIdleWakeupsCallback on_idle_wakeups;
    OnPhysicalMemoryCallback on_physical_memory;
    OnStartTimeCallback on_start_time;
    OnCpuTimeCallback on_cpu_time;

   private:
    DISALLOW_COPY_AND_ASSIGN(Callbacks);
  };

  typedef std::map<base::ProcessId, Callbacks> CallbacksMap;

  // Contains all results of refresh for a single process.
  struct RefreshResult {
    base::ProcessId process_id;
    int idle_wakeups_per_second;
    int64_t physical_bytes;
    base::Time start_time;
    base::TimeDelta cpu_time;
  };

  typedef std::vector<RefreshResult> RefreshResults;

  // Posted on the worker thread to do the actual refresh.
  std::unique_ptr<RefreshResults> RefreshOnWorkerThread();

  // Called on UI thread when the refresh is done.
  void OnRefreshDone(std::unique_ptr<RefreshResults> refresh_results);

  // Clear cached data.
  void ClearState();

  // Used to filter process information.
  static std::vector<base::FilePath> GetSupportedImageNames();
  bool IsSupportedImageName(base::FilePath::StringPieceType image_name) const;

  // Captures a snapshot of data for all chrome processes.
  // Runs on the worker thread.
  std::unique_ptr<ProcessDataSnapshot> CaptureSnapshot();

  // Produce refresh results by diffing two snapshots.
  static void MakeResultsFromTwoSnapshots(
      const ProcessDataSnapshot& prev_snapshot,
      const ProcessDataSnapshot& snapshot,
      RefreshResults* results);

  // Produce refresh results from one snapshot.
  // This is used only the first time when only one snapshot is available.
  static void MakeResultsFromSnapshot(
      const ProcessDataSnapshot& snapshot,
      RefreshResults* results);

  // Accumulates callbacks passed from TaskGroup objects passed via
  // RegisterCallbacks calls.
  CallbacksMap callbacks_map_;

  // Refresh flags passed via Refresh.
  int64_t refresh_flags_;

  // Snapshot of previously captured resources used to calculate the delta.
  std::unique_ptr<ProcessDataSnapshot> previous_snapshot_;

  // Size of the buffer previously used to query system information.
  size_t previous_buffer_size_;

  // Image names that CaptureSnapshot uses to filter processes.
  const std::vector<base::FilePath> supported_image_names_;

  // The specific blocking pool SequencedTaskRunner that will be used to post
  // the refresh tasks onto serially.
  scoped_refptr<base::SequencedTaskRunner> blocking_pool_runner_;

  // To assert we're running on the correct thread.
  base::SequenceChecker worker_pool_sequenced_checker_;
#endif  // defined(OS_WIN)

  DISALLOW_COPY_AND_ASSIGN(SharedSampler);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_SAMPLING_SHARED_SAMPLER_H_
