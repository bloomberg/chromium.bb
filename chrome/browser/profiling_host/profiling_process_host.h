// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
#define CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/profiling/memlog.mojom.h"
#include "chrome/common/profiling/memlog_client.h"
#include "chrome/common/profiling/memlog_client.mojom.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "services/service_manager/public/cpp/connector.h"

namespace base {
class FilePath;
}

namespace profiling {

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
  enum class Mode {
    // No profiling enabled.
    kNone,

    // Only profile the browser process.
    kBrowser,

    // Profile all processes.
    kAll,
  };

  // Returns the mode set on the current process' command line.
  static Mode GetCurrentMode();

  // Launches the profiling process if necessary and returns a pointer to it.
  static ProfilingProcessHost* EnsureStarted(
      content::ServiceManagerConnection* connection,
      Mode mode);

  // Returns a pointer to the current global profiling process host.
  static ProfilingProcessHost* GetInstance();

  // Sends a message to the profiling process that it dump the given process'
  // memory data to the given file.
  void RequestProcessDump(base::ProcessId pid, const base::FilePath& dest);

  // Sends a message to the profiling process that it report the given process'
  // memory data to the crash server (slow-report).
  void RequestProcessReport(base::ProcessId pid);

 private:
  friend struct base::DefaultSingletonTraits<ProfilingProcessHost>;
  ProfilingProcessHost();
  ~ProfilingProcessHost() override;

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

  void OnDumpProcessForTracingCallback(mojo::ScopedSharedBufferHandle buffer,
                                       uint32_t size);

  // Starts the profiling process.
  void LaunchAsService();

  // Sends the receiving end of the data pipe to the profiling service.
  void SendPipeToProfilingService(
      profiling::mojom::MemlogClientPtr memlog_client,
      base::ProcessId pid);
  // Sends the sending end of the data pipe to the client process.
  void SendPipeToClientProcess(profiling::mojom::MemlogClientPtr memlog_client,
                               mojo::ScopedHandle handle);

  void GetOutputFileOnBlockingThread(base::ProcessId pid,
                                     const base::FilePath& dest,
                                     bool upload);
  void HandleDumpProcessOnIOThread(base::ProcessId pid,
                                   base::FilePath file_path,
                                   base::File file,
                                   bool upload);
  void OnProcessDumpComplete(base::FilePath file_path,
                             bool upload,
                             bool success);

  void SetMode(Mode mode);

  // Returns the metadata for the trace. This is the minimum amount of metadata
  // needed to symbolize the trace.
  std::unique_ptr<base::DictionaryValue> GetMetadataJSONForTrace();

  content::NotificationRegistrar registrar_;
  std::unique_ptr<service_manager::Connector> connector_;
  mojom::MemlogPtr memlog_;

  // The mode determines which processes should be profiled.
  Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(ProfilingProcessHost);
};

}  // namespace profiling

#endif  // CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
