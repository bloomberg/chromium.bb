// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
#define CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "chrome/browser/profiling_host/background_profiling_triggers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/profiling/profiling_client.h"
#include "chrome/common/profiling/profiling_client.mojom.h"
#include "chrome/common/profiling/profiling_service.mojom.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "services/service_manager/public/cpp/connector.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class RenderProcessHost;
}  // namespace content

namespace profiling {

extern const base::Feature kOOPHeapProfilingFeature;
extern const char kOOPHeapProfilingFeatureMode[];

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
//
// TODO(ajwong): This host class seems over kill at this point. Can this be
// fully subsumed by the ProfilingService class?
class ProfilingProcessHost : public content::BrowserChildProcessObserver,
                             content::NotificationObserver,
                             base::trace_event::MemoryDumpProvider {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Mode {
    // No profiling enabled.
    kNone = 0,

    // Only profile the browser and GPU processes.
    kMinimal = 1,

    // Profile all processes.
    kAll = 2,

    // Profile only the browser process.
    kBrowser = 3,

    // Profile only the gpu process.
    kGpu = 4,

    // Profile a sampled number of renderer processes.
    kRendererSampling = 5,

    kCount
  };

  // Returns the mode.
  Mode mode() const { return mode_; }

  // Returns the mode set on the current process' command line.
  static Mode GetCurrentMode();
  static Mode ConvertStringToMode(const std::string& input);
  bool ShouldProfileProcessType(int process_type);

  // Launches the profiling process and returns a pointer to it.
  static ProfilingProcessHost* Start(
      content::ServiceManagerConnection* connection,
      Mode mode);

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
  // asynchronously on the UI thread. If
  // |stop_immediately_after_heap_dump_for_tests| is true, then |callback| will
  // be called as soon as the heap dump is added to the trace. Otherwise,
  // |callback| will be called after 10s. This gives time for the
  // MemoryDumpProviders to dump to the trace, which is asynchronous and has no
  // finish notification. This intentionally avoids waiting for the heap-dump
  // finished signal, in case there's a problem with the profiling process and
  // the heap-dump is never added to the trace.
  // Public for testing.
  void RequestTraceWithHeapDump(
      TraceFinishedCallback callback,
      bool stop_immediately_after_heap_dump_for_tests);

  // Returns the pids of all profiled processes. The callback is posted on the
  // UI thread.
  using GetProfiledPidsCallback =
      base::OnceCallback<void(std::vector<base::ProcessId> pids)>;
  void GetProfiledPids(GetProfiledPidsCallback callback);

  // Starts profiling the process with the given id.
  void StartProfiling(base::ProcessId pid);

 private:
  friend struct base::DefaultSingletonTraits<ProfilingProcessHost>;
  friend class BackgroundProfilingTriggersTest;
  friend class MemlogBrowserTest;
  friend class ProfilingTestDriver;
  FRIEND_TEST_ALL_PREFIXES(ProfilingProcessHost, ShouldProfileNewRenderer);

  ProfilingProcessHost();
  ~ProfilingProcessHost() override;

  void Register();
  void Unregister();

  // Set the profiling mode. Exposed for unittests.
  void SetMode(Mode mode);

  // Make and store a connector from |connection|.
  void MakeConnector(content::ServiceManagerConnection* connection);

  // BrowserChildProcessObserver
  // Observe connection of non-renderer child processes.
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;

  // NotificationObserver
  // Observe connection of renderer child processes.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // base::trace_event::MemoryDumpProvider
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;
  void OnMemoryDumpOnIOThread(uint64_t dump_guid);

  void OnDumpProcessesForTracingCallback(
      uint64_t guid,
      std::vector<profiling::mojom::SharedBufferWithSizePtr> buffers);

  // Starts the profiling process.
  void LaunchAsService();

  // Called on the UI thread after the heap dump has been added to the trace.
  void DumpProcessFinishedUIThread();

  // Must be called on the UI thread.
  // Called after the MDPs have been given time to emit memory dumps.
  void OnMinimumTimeHasElapsed();

  // Sends the end of the data pipe to the profiling service.
  void AddClientToProfilingService(profiling::mojom::ProfilingClientPtr client,
                                   base::ProcessId pid,
                                   profiling::mojom::ProcessType process_type);

  void SaveTraceToFileOnBlockingThread(base::FilePath dest,
                                       std::string trace,
                                       SaveTraceFinishedCallback done);

  // Reports the profiling mode.
  void ReportMetrics();

  // Helpers for controlling process selection for the sampling modes.
  bool ShouldProfileNewRenderer(content::RenderProcessHost* renderer) const;

  // Sets up the profiling connection for the given child process.
  void StartProfilingNonRendererChild(const content::ChildProcessData&);
  void StartProfilingNonRendererChildOnIOThread(
      int child_process_id,
      base::ProcessId proc_id,
      profiling::mojom::ProcessType process_type);

  void StartProfilingRenderer(content::RenderProcessHost* host);

  // SetRenderer.
  void SetRendererSamplingAlwaysProfileForTest();

  void GetProfiledPidsOnIOThread(GetProfiledPidsCallback callback);
  void StartProfilingPidOnIOThread(base::ProcessId pid);

  bool TakingTraceForUpload();
  bool SetTakingTraceForUpload(bool new_state);

  content::NotificationRegistrar registrar_;
  std::unique_ptr<service_manager::Connector> connector_;
  mojom::ProfilingServicePtr profiling_service_;

  // Whether or not the host is registered to the |registrar_|.
  bool is_registered_;

  // Handle background triggers on high memory pressure. A trigger will call
  // |RequestProcessReport| on this instance.
  BackgroundProfilingTriggers background_triggers_;

  // Every 24-hours, reports the profiling mode.
  base::RepeatingTimer metrics_timer_;

  // The mode determines which processes should be profiled.
  Mode mode_;

  // Whether or not the profiling host is started.
  static bool has_started_;

  // This is used to identify the currently profiled renderers. Elements should
  // only be accessed on the UI thread and their values should be considered
  // opaque.
  //
  // Semantically, the elements must be something that identifies which
  // specific RenderProcess is being profiled. When the underlying RenderProcess
  // goes away, the element must be removed. The RenderProcessHost
  // pointer and the NOTIFICATION_RENDERER_PROCESS_CREATED notification can be
  // used to provide these semantics.
  //
  // This variable represents renderers that have been instructed to start
  // profiling - it does not reflect whether a renderer is currently still being
  // profiled. That information is only known by the profiling service, and for
  // simplicity, it's easier to just track this variable in this process.
  std::unordered_set<void*> profiled_renderers_;

  // For Tests. In kRendererSampling mode, overrides sampling to always profile
  // a renderer process if one is already not going.
  bool always_sample_for_tests_;

  // Must only ever be used from the UI thread. Will be called after the
  // profiling process dumps heaps into the trace log and MDPs have been given
  // time to do the same.
  base::OnceClosure finish_tracing_callback_;

  // True if the instance is attempting to take a trace to upload to the crash
  // servers. Pruning of small allocations is always enabled for these traces.
  bool taking_trace_for_upload_ = false;

  // Guards |taking_trace_for_upload_|.
  base::Lock taking_trace_for_upload_lock_;

  // If the instance has started a trace, the trace should be stopped when both
  // members become true. Both members must only be accessed on the UI thread.
  bool minimum_time_has_elapsed_ = false;
  bool heap_dump_has_been_added_ = false;

  DISALLOW_COPY_AND_ASSIGN(ProfilingProcessHost);
};

}  // namespace profiling

#endif  // CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
