// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
#define CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "chrome/browser/profiling_host/background_profiling_triggers.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class ServiceManagerConnection;
}  // namespace content

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace heap_profiling {

class ChromeClientConnectionManager;
class Controller;
enum class Mode;

// Represents the browser side of the profiling process (//chrome/profiling).
//
// The profiling process is a singleton. The profiling process infrastructure
// is implemented in the Chrome layer so doesn't depend on the content
// infrastructure for managing child processes.
//
// Not thread safe. Should be used on the browser UI thread only.
//
// The profiling process host can be started normally while Chrome is running,
// but can also start in a partial mode where the memory logging connections
// are active but the Mojo control channel has not yet been connected. This is
// to support starting the profiling process very early in the startup
// process (to get most memory events) before other infrastructure like the
// I/O thread has been started.
class ProfilingProcessHost {
 public:
  // Returns the mode specified by the command line or via about://flags.
  Mode GetMode();

  // Launches the heap profiling service and connects clients. Must be called
  // from the UI thread. This requires hopping to the IO thread and back.
  // |callback| will be called on the UI thread after this is finished.
  static void Start(content::ServiceManagerConnection* connection,
                    Mode mode,
                    mojom::StackMode stack_mode,
                    uint32_t sampling_rate,
                    base::OnceClosure callback);

  // Returns true if Start() has been called.
  static bool has_started() { return has_started_; }

  // Returns a pointer to the current global profiling process host.
  static ProfilingProcessHost* GetInstance();

  void ConfigureBackgroundProfilingTriggers();

  // Create a trace with a heap dump at the given path.
  // This is equivalent to navigating to chrome://tracing, taking a trace with
  // only the memory-infra category selected, waiting 10 seconds, and saving the
  // result to |dest|.
  // |done| will be called on the UI thread.
  using SaveTraceFinishedCallback = base::OnceCallback<void(bool success)>;
  void SaveTraceWithHeapDumpToFile(
      base::FilePath dest,
      SaveTraceFinishedCallback done,
      bool stop_immediately_after_heap_dump_for_tests);

  // Sends a message to the profiling process to report all profiled processes
  // memory data to the crash server (slow-report).
  void RequestProcessReport(std::string trigger_name);

  using TraceFinishedCallback =
      base::OnceCallback<void(bool success, std::string trace_json)>;

  // This method must be called from the UI thread. |callback| will be called
  // asynchronously on the UI thread.
  //
  // This function does the following:
  //   1. Starts tracing with no categories enabled.
  //   2. Requests and waits for memory_instrumentation service to dump to
  //   trace.
  //   3. Stops tracing.
  //
  // Public for testing.
  void RequestTraceWithHeapDump(TraceFinishedCallback callback,
                                bool anonymize);

  // Returns the pids of all profiled processes. The callback is posted on the
  // UI thread.
  using GetProfiledPidsCallback =
      base::OnceCallback<void(std::vector<base::ProcessId> pids)>;
  void GetProfiledPids(GetProfiledPidsCallback callback);

  // Starts profiling the process with the given id.
  void StartManualProfiling(base::ProcessId pid);

  // Public for testing. Controls whether the profiling service keeps small
  // allocations in heap dumps.
  void SetKeepSmallAllocations(bool keep_small_allocations);

 private:
  friend struct base::DefaultSingletonTraits<ProfilingProcessHost>;
  friend class BackgroundProfilingTriggersTest;
  friend class MemlogBrowserTest;
  friend class ProfilingTestDriver;

  ProfilingProcessHost();
  ~ProfilingProcessHost();

  // Starts the profiling process.
  void StartServiceOnIOThread(
      mojom::StackMode stack_mode,
      uint32_t sampling_rate,
      std::unique_ptr<service_manager::Connector> connector,
      Mode mode,
      base::OnceClosure closure);

  void FinishPostServiceSetupOnUIThread(
      Mode mode,
      base::OnceClosure closure,
      base::WeakPtr<Controller> controller_weak_ptr);

  // Called on the UI thread after the heap dump has been added to the trace.
  void DumpProcessFinishedUIThread();

  void SaveTraceToFileOnBlockingThread(base::FilePath dest,
                                       std::string trace,
                                       SaveTraceFinishedCallback done);

  // Reports the profiling mode.
  void ReportMetrics();

  void GetProfiledPidsOnIOThread(GetProfiledPidsCallback callback);
  void StartProfilingPidOnIOThread(base::ProcessId pid);

  bool TakingTraceForUpload();
  bool SetTakingTraceForUpload(bool new_state);

  // Bound to the IO thread.
  std::unique_ptr<service_manager::Connector> connector_;

  // Bound to the IO thread.
  std::unique_ptr<Controller> controller_;

  // Bound to the UI thread.
  std::unique_ptr<ChromeClientConnectionManager> client_connection_manager_;

  // Handle background triggers on high memory pressure. A trigger will call
  // |RequestProcessReport| on this instance.
  BackgroundProfilingTriggers background_triggers_;

  // Every 24-hours, reports the profiling mode.
  base::RepeatingTimer metrics_timer_;

  bool should_sample_ = false;
  uint32_t sampling_rate_ = 1;

  // Whether or not the profiling host is started.
  static bool has_started_;

  // True if the instance is attempting to take a trace to upload to the crash
  // servers. Pruning of small allocations is always enabled for these traces.
  bool taking_trace_for_upload_ = false;

  // Guards |taking_trace_for_upload_|.
  base::Lock taking_trace_for_upload_lock_;

  DISALLOW_COPY_AND_ASSIGN(ProfilingProcessHost);
};

}  // namespace heap_profiling

#endif  // CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
