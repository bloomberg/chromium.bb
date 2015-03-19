// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_DUMP_MANAGER_H_
#define BASE_TRACE_EVENT_MEMORY_DUMP_MANAGER_H_

#include <vector>

#include "base/atomicops.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"

namespace base {
namespace trace_event {

class MemoryDumpProvider;

// Captures the reason why a dump point is being requested. This is to allow
// selective enabling of dump points, filtering and post-processing.
enum class DumpPointType {
  TASK_BEGIN,         // Dumping memory at the beginning of a message-loop task.
  TASK_END,           // Dumping memory at the ending of a message-loop task.
  PERIODIC_INTERVAL,  // Dumping memory at periodic intervals.
  EXPLICITLY_TRIGGERED,  // Non maskable dump request.
};

// This is the interface exposed to the rest of the codebase to deal with
// memory tracing. The main entry point for clients is represented by
// RequestDumpPoint(). The extension by Un(RegisterDumpProvider).
class BASE_EXPORT MemoryDumpManager : public TraceLog::EnabledStateObserver {
 public:
  static MemoryDumpManager* GetInstance();

  // Invoked once per process to register the TraceLog observer.
  void Initialize();

  // MemoryDumpManager does NOT take memory ownership of |mdp|, which is
  // expected to be a singleton.
  void RegisterDumpProvider(MemoryDumpProvider* mdp);
  void UnregisterDumpProvider(MemoryDumpProvider* mdp);

  // Requests a memory dump. The dump might happen or not depending on the
  // filters and categories specified when enabling tracing.
  void RequestDumpPoint(DumpPointType dump_point_type);

  // TraceLog::EnabledStateObserver implementation.
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  // Returns the MemoryDumpProvider which is currently being dumping into a
  // ProcessMemoryDump via DumpInto(...) if any, nullptr otherwise.
  MemoryDumpProvider* dump_provider_currently_active() const {
    return dump_provider_currently_active_;
  }

 private:
  friend struct DefaultDeleter<MemoryDumpManager>;  // For the testing instance.
  friend struct DefaultSingletonTraits<MemoryDumpManager>;
  friend class MemoryDumpManagerTest;

  static const char kTraceCategory[];

  static void SetInstanceForTesting(MemoryDumpManager* instance);

  MemoryDumpManager();
  virtual ~MemoryDumpManager();

  // Broadcasts the dump requests to the other processes.
  void BroadcastDumpRequest();

  // Creates a dump point for the current process and appends it to the trace.
  void CreateLocalDumpPoint(DumpPointType dump_point_type, uint64 guid);

  std::vector<MemoryDumpProvider*> dump_providers_registered_;  // Not owned.
  std::vector<MemoryDumpProvider*> dump_providers_enabled_;     // Not owned.

  // TODO(primiano): this is required only until crbug.com/466121 gets fixed.
  MemoryDumpProvider* dump_provider_currently_active_;  // Now owned.

  // Protects from concurrent accesses to the |dump_providers_*|, e.g., tearing
  // down logging while creating a dump point on another thread.
  Lock lock_;

  // Optimization to avoid attempting any dump point (i.e. to not walk an empty
  // dump_providers_enabled_ list) when tracing is not enabled.
  subtle::AtomicWord memory_tracing_enabled_;

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpManager);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_DUMP_MANAGER_H_
