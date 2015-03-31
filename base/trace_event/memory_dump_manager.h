// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_DUMP_MANAGER_H_
#define BASE_TRACE_EVENT_MEMORY_DUMP_MANAGER_H_

#include <vector>

#include "base/atomicops.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"

namespace base {
namespace trace_event {

class MemoryDumpManagerDelegate;
class MemoryDumpProvider;

// This is the interface exposed to the rest of the codebase to deal with
// memory tracing. The main entry point for clients is represented by
// RequestDumpPoint(). The extension by Un(RegisterDumpProvider).
class BASE_EXPORT MemoryDumpManager : public TraceLog::EnabledStateObserver {
 public:
  static MemoryDumpManager* GetInstance();

  // Invoked once per process to register the TraceLog observer.
  void Initialize();

  // See the lifetime and thread-safety requirements on the delegate below in
  // the |MemoryDumpManagerDelegate| docstring.
  void SetDelegate(MemoryDumpManagerDelegate* delegate);

  // MemoryDumpManager does NOT take memory ownership of |mdp|, which is
  // expected to be a singleton.
  void RegisterDumpProvider(MemoryDumpProvider* mdp);
  void UnregisterDumpProvider(MemoryDumpProvider* mdp);

  // Requests a memory dump. The dump might happen or not depending on the
  // filters and categories specified when enabling tracing.
  // The optional |callback| is executed asynchronously, on an arbitrary thread,
  // to notify about the completion of the global dump (i.e. after all the
  // processes have dumped) and its success (true iff all the dumps were
  // successful).
  void RequestGlobalDump(MemoryDumpType dump_type,
                         const MemoryDumpCallback& callback);

  // Same as above, but without callback.
  void RequestGlobalDump(MemoryDumpType dump_type);

  // Creates a memory dump for the current process and appends it to the trace.
  void CreateProcessDump(const MemoryDumpRequestArgs& args);

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

  std::vector<MemoryDumpProvider*> dump_providers_registered_;  // Not owned.
  std::vector<MemoryDumpProvider*> dump_providers_enabled_;     // Not owned.

  // TODO(primiano): this is required only until crbug.com/466121 gets fixed.
  MemoryDumpProvider* dump_provider_currently_active_;  // Not owned.

  MemoryDumpManagerDelegate* delegate_;  // Not owned.

  // Protects from concurrent accesses to the |dump_providers_*| and |delegate_|
  // to guard against disabling logging while dumping on another thread.
  Lock lock_;

  // Optimization to avoid attempting any memory dump (i.e. to not walk an empty
  // dump_providers_enabled_ list) when tracing is not enabled.
  subtle::AtomicWord memory_tracing_enabled_;

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpManager);
};

// The delegate is supposed to be long lived (read: a Singleton) and thread
// safe (i.e. should expect calls from any thread and handle thread hopping).
class BASE_EXPORT MemoryDumpManagerDelegate {
 public:
  virtual void RequestGlobalMemoryDump(const MemoryDumpRequestArgs& args,
                                       const MemoryDumpCallback& callback) = 0;

 protected:
  MemoryDumpManagerDelegate() {}
  virtual ~MemoryDumpManagerDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryDumpManagerDelegate);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_DUMP_MANAGER_H_
