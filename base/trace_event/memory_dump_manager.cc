// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_manager.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_argument.h"

namespace base {
namespace trace_event {

namespace {
MemoryDumpManager* g_instance_for_testing = nullptr;
}

// TODO(primiano): this should be smarter and should do something similar to
// trace event synthetic delays.
const char MemoryDumpManager::kTraceCategory[] =
    TRACE_DISABLED_BY_DEFAULT("memory-dumps");

// static
MemoryDumpManager* MemoryDumpManager::GetInstance() {
  if (g_instance_for_testing)
    return g_instance_for_testing;

  return Singleton<MemoryDumpManager,
                   LeakySingletonTraits<MemoryDumpManager>>::get();
}

// static
void MemoryDumpManager::SetInstanceForTesting(MemoryDumpManager* instance) {
  g_instance_for_testing = instance;
}

MemoryDumpManager::MemoryDumpManager() : memory_tracing_enabled_(0) {
}

MemoryDumpManager::~MemoryDumpManager() {
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
}

void MemoryDumpManager::Initialize() {
  TRACE_EVENT0(kTraceCategory, "init");  // Add to trace-viewer category list.
  trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
}

void MemoryDumpManager::RegisterDumpProvider(MemoryDumpProvider* mdp) {
  AutoLock lock(lock_);
  if (std::find(dump_providers_registered_.begin(),
                dump_providers_registered_.end(),
                mdp) != dump_providers_registered_.end()) {
    return;
  }
  dump_providers_registered_.push_back(mdp);
}

void MemoryDumpManager::UnregisterDumpProvider(MemoryDumpProvider* mdp) {
  AutoLock lock(lock_);

  // Remove from the registered providers list.
  auto it = std::find(dump_providers_registered_.begin(),
                      dump_providers_registered_.end(), mdp);
  if (it != dump_providers_registered_.end())
    dump_providers_registered_.erase(it);

  // Remove from the enabled providers list. This is to deal with the case that
  // UnregisterDumpProvider is called while the trace is enabled.
  it = std::find(dump_providers_enabled_.begin(), dump_providers_enabled_.end(),
                 mdp);
  if (it != dump_providers_enabled_.end())
    dump_providers_enabled_.erase(it);
}

void MemoryDumpManager::RequestDumpPoint(DumpPointType type) {
  // TODO(primiano): this will have more logic, IPC broadcast & co.
  // Bail out immediately if tracing is not enabled at all.
  if (!UNLIKELY(subtle::NoBarrier_Load(&memory_tracing_enabled_)))
    return;

  CreateLocalDumpPoint();
}

void MemoryDumpManager::BroadcastDumpRequest() {
  NOTREACHED();  // TODO(primiano): implement IPC synchronization.
}

// Creates a dump point for the current process and appends it to the trace.
void MemoryDumpManager::CreateLocalDumpPoint() {
  AutoLock lock(lock_);
  bool did_any_provider_dump = false;
  scoped_ptr<ProcessMemoryDump> pmd(new ProcessMemoryDump());

  for (auto it = dump_providers_enabled_.begin();
       it != dump_providers_enabled_.end();) {
    MemoryDumpProvider* dump_provider = *it;
    if (!dump_provider->DumpInto(pmd.get())) {
      LOG(ERROR) << "The memory dumper " << dump_provider->GetFriendlyName()
                 << " failed, possibly due to sandboxing (crbug.com/461788), "
                    "disabling it for current process. Try restarting chrome "
                    "with the --no-sandbox switch.";
      it = dump_providers_enabled_.erase(it);
    } else {
      did_any_provider_dump = true;
      ++it;
    }
  }

  // Don't create a dump point if all the dumpers failed.
  if (!did_any_provider_dump)
    return;

  scoped_refptr<TracedValue> value(new TracedValue());
  pmd->AsValueInto(value.get());
  // TODO(primiano): add the dump point to the trace at this point.
}

void MemoryDumpManager::OnTraceLogEnabled() {
  // TODO(primiano): at this point we query  TraceLog::GetCurrentCategoryFilter
  // to figure out (and cache) which dumpers should be enabled or not.
  // For the moment piggy back everything on the generic "memory" category.
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kTraceCategory, &enabled);

  AutoLock lock(lock_);
  if (enabled) {
    dump_providers_enabled_.assign(dump_providers_registered_.begin(),
                                   dump_providers_registered_.end());
  } else {
    dump_providers_enabled_.clear();
  }
  subtle::NoBarrier_Store(&memory_tracing_enabled_, 1);
}

void MemoryDumpManager::OnTraceLogDisabled() {
  AutoLock lock(lock_);
  dump_providers_enabled_.clear();
  subtle::NoBarrier_Store(&memory_tracing_enabled_, 0);
}

}  // namespace trace_event
}  // namespace base
