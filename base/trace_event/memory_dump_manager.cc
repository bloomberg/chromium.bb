// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_manager.h"

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/heap_profiler_stack_frame_deduplicator.h"
#include "base/trace_event/heap_profiler_type_name_deduplicator.h"
#include "base/trace_event/malloc_dump_provider.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_session_state.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_argument.h"
#include "build/build_config.h"

#if !defined(OS_NACL)
#include "base/trace_event/process_memory_totals_dump_provider.h"
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "base/trace_event/process_memory_maps_dump_provider.h"
#endif

#if defined(OS_ANDROID)
#include "base/trace_event/java_heap_dump_provider_android.h"
#endif

#if defined(OS_WIN)
#include "base/trace_event/winheap_dump_provider_win.h"
#endif

namespace base {
namespace trace_event {

namespace {

const int kTraceEventNumArgs = 1;
const char* kTraceEventArgNames[] = {"dumps"};
const unsigned char kTraceEventArgTypes[] = {TRACE_VALUE_TYPE_CONVERTABLE};

StaticAtomicSequenceNumber g_next_guid;
uint32_t g_periodic_dumps_count = 0;
uint32_t g_heavy_dumps_rate = 0;
MemoryDumpManager* g_instance_for_testing = nullptr;

void RequestPeriodicGlobalDump() {
  MemoryDumpLevelOfDetail level_of_detail;
  if (g_heavy_dumps_rate == 0) {
    level_of_detail = MemoryDumpLevelOfDetail::LIGHT;
  } else {
    level_of_detail = g_periodic_dumps_count == 0
                          ? MemoryDumpLevelOfDetail::DETAILED
                          : MemoryDumpLevelOfDetail::LIGHT;

    if (++g_periodic_dumps_count == g_heavy_dumps_rate)
      g_periodic_dumps_count = 0;
  }

  MemoryDumpManager::GetInstance()->RequestGlobalDump(
      MemoryDumpType::PERIODIC_INTERVAL, level_of_detail);
}

// Callback wrapper to hook upon the completion of RequestGlobalDump() and
// inject trace markers.
void OnGlobalDumpDone(MemoryDumpCallback wrapped_callback,
                      uint64_t dump_guid,
                      bool success) {
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      MemoryDumpManager::kTraceCategory, "GlobalMemoryDump",
      TRACE_ID_MANGLE(dump_guid), "success", success);

  if (!wrapped_callback.is_null()) {
    wrapped_callback.Run(dump_guid, success);
    wrapped_callback.Reset();
  }
}

}  // namespace

// static
const char* const MemoryDumpManager::kTraceCategory =
    TRACE_DISABLED_BY_DEFAULT("memory-infra");

// static
const int MemoryDumpManager::kMaxConsecutiveFailuresCount = 3;

// static
const uint64_t MemoryDumpManager::kInvalidTracingProcessId = 0;

// static
const char* const MemoryDumpManager::kSystemAllocatorPoolName =
#if defined(MALLOC_MEMORY_TRACING_SUPPORTED)
    MallocDumpProvider::kAllocatedObjects;
#elif defined(OS_WIN)
    WinHeapDumpProvider::kAllocatedObjects;
#else
    nullptr;
#endif

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

MemoryDumpManager::MemoryDumpManager()
    : delegate_(nullptr),
      is_coordinator_(false),
      memory_tracing_enabled_(0),
      tracing_process_id_(kInvalidTracingProcessId),
      dumper_registrations_ignored_for_testing_(false) {
  g_next_guid.GetNext();  // Make sure that first guid is not zero.

  heap_profiling_enabled_ = CommandLine::InitializedForCurrentProcess()
                                ? CommandLine::ForCurrentProcess()->HasSwitch(
                                      switches::kEnableHeapProfiling)
                                : false;

  if (heap_profiling_enabled_)
    AllocationContextTracker::SetCaptureEnabled(true);
}

MemoryDumpManager::~MemoryDumpManager() {
  TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
}

void MemoryDumpManager::Initialize(MemoryDumpManagerDelegate* delegate,
                                   bool is_coordinator) {
  {
    AutoLock lock(lock_);
    DCHECK(delegate);
    DCHECK(!delegate_);
    delegate_ = delegate;
    is_coordinator_ = is_coordinator;
  }

// Enable the core dump providers.
#if !defined(OS_NACL)
  RegisterDumpProvider(ProcessMemoryTotalsDumpProvider::GetInstance(),
                       "ProcessMemoryTotals", nullptr);
#endif

#if defined(MALLOC_MEMORY_TRACING_SUPPORTED)
  RegisterDumpProvider(MallocDumpProvider::GetInstance(), "Malloc", nullptr);
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
  RegisterDumpProvider(ProcessMemoryMapsDumpProvider::GetInstance(),
                       "ProcessMemoryMaps", nullptr);
#endif

#if defined(OS_ANDROID)
  RegisterDumpProvider(JavaHeapDumpProvider::GetInstance(), "JavaHeap",
                       nullptr);
#endif

#if defined(OS_WIN)
  RegisterDumpProvider(WinHeapDumpProvider::GetInstance(), "WinHeap", nullptr);
#endif

  // If tracing was enabled before initializing MemoryDumpManager, we missed the
  // OnTraceLogEnabled() event. Synthetize it so we can late-join the party.
  bool is_tracing_already_enabled = TraceLog::GetInstance()->IsEnabled();
  TRACE_EVENT0(kTraceCategory, "init");  // Add to trace-viewer category list.
  TraceLog::GetInstance()->AddEnabledStateObserver(this);
  if (is_tracing_already_enabled)
    OnTraceLogEnabled();
}

void MemoryDumpManager::RegisterDumpProvider(
    MemoryDumpProvider* mdp,
    const char* name,
    const scoped_refptr<SingleThreadTaskRunner>& task_runner,
    const MemoryDumpProvider::Options& options) {
  if (dumper_registrations_ignored_for_testing_)
    return;

  scoped_refptr<MemoryDumpProviderInfo> mdpinfo =
      new MemoryDumpProviderInfo(mdp, name, task_runner, options);

  {
    AutoLock lock(lock_);
    bool already_registered = !dump_providers_.insert(mdpinfo).second;
    // This actually happens in some tests which don't have a clean tear-down
    // path for RenderThreadImpl::Init().
    if (already_registered)
      return;
  }

  if (heap_profiling_enabled_)
    mdp->OnHeapProfilingEnabled(true);
}

void MemoryDumpManager::RegisterDumpProvider(
    MemoryDumpProvider* mdp,
    const char* name,
    const scoped_refptr<SingleThreadTaskRunner>& task_runner) {
  RegisterDumpProvider(mdp, name, task_runner, MemoryDumpProvider::Options());
}

void MemoryDumpManager::UnregisterDumpProvider(MemoryDumpProvider* mdp) {
  UnregisterDumpProviderInternal(mdp, false /* delete_async */);
}

void MemoryDumpManager::UnregisterAndDeleteDumpProviderSoon(
    scoped_ptr<MemoryDumpProvider> mdp) {
  UnregisterDumpProviderInternal(mdp.release(), true /* delete_async */);
}

void MemoryDumpManager::UnregisterDumpProviderInternal(
    MemoryDumpProvider* mdp,
    bool take_mdp_ownership_and_delete_async) {
  scoped_ptr<MemoryDumpProvider> owned_mdp;
  if (take_mdp_ownership_and_delete_async)
    owned_mdp.reset(mdp);

  AutoLock lock(lock_);

  auto mdp_iter = dump_providers_.begin();
  for (; mdp_iter != dump_providers_.end(); ++mdp_iter) {
    if ((*mdp_iter)->dump_provider == mdp)
      break;
  }

  if (mdp_iter == dump_providers_.end())
    return;  // Not registered / already unregistered.

  if (take_mdp_ownership_and_delete_async) {
    // The MDP will be deleted whenever the MDPInfo struct will, that is either:
    // - At the end of this function, if no dump is in progress.
    // - In the prologue of the ContinueAsyncProcessDump().
    DCHECK(!(*mdp_iter)->owned_dump_provider);
    (*mdp_iter)->owned_dump_provider = std::move(owned_mdp);
  } else if (subtle::NoBarrier_Load(&memory_tracing_enabled_)) {
    // If you hit this DCHECK, your dump provider has a bug.
    // Unregistration of a MemoryDumpProvider is safe only if:
    // - The MDP has specified a thread affinity (via task_runner()) AND
    //   the unregistration happens on the same thread (so the MDP cannot
    //   unregister and be in the middle of a OnMemoryDump() at the same time.
    // - The MDP has NOT specified a thread affinity and its ownership is
    //   transferred via UnregisterAndDeleteDumpProviderSoon().
    // In all the other cases, it is not possible to guarantee that the
    // unregistration will not race with OnMemoryDump() calls.
    DCHECK((*mdp_iter)->task_runner &&
           (*mdp_iter)->task_runner->BelongsToCurrentThread())
        << "MemoryDumpProvider \"" << (*mdp_iter)->name << "\" attempted to "
        << "unregister itself in a racy way. Please file a crbug.";
  }

  // The MDPInfo instance can still be referenced by the
  // |ProcessMemoryDumpAsyncState.pending_dump_providers|. For this reason
  // the MDPInfo is flagged as disabled. It will cause ContinueAsyncProcessDump
  // to just skip it, without actually invoking the |mdp|, which might be
  // destroyed by the caller soon after this method returns.
  (*mdp_iter)->disabled = true;
  dump_providers_.erase(mdp_iter);
}

void MemoryDumpManager::RequestGlobalDump(
    MemoryDumpType dump_type,
    MemoryDumpLevelOfDetail level_of_detail,
    const MemoryDumpCallback& callback) {
  // Bail out immediately if tracing is not enabled at all.
  if (!UNLIKELY(subtle::NoBarrier_Load(&memory_tracing_enabled_))) {
    if (!callback.is_null())
      callback.Run(0u /* guid */, false /* success */);
    return;
  }

  const uint64_t guid =
      TraceLog::GetInstance()->MangleEventId(g_next_guid.GetNext());

  // Creates an async event to keep track of the global dump evolution.
  // The |wrapped_callback| will generate the ASYNC_END event and then invoke
  // the real |callback| provided by the caller.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kTraceCategory, "GlobalMemoryDump",
                                    TRACE_ID_MANGLE(guid));
  MemoryDumpCallback wrapped_callback = Bind(&OnGlobalDumpDone, callback);

  // Technically there is no need to grab the |lock_| here as the delegate is
  // long-lived and can only be set by Initialize(), which is locked and
  // necessarily happens before memory_tracing_enabled_ == true.
  // Not taking the |lock_|, though, is lakely make TSan barf and, at this point
  // (memory-infra is enabled) we're not in the fast-path anymore.
  MemoryDumpManagerDelegate* delegate;
  {
    AutoLock lock(lock_);
    delegate = delegate_;
  }

  // The delegate will coordinate the IPC broadcast and at some point invoke
  // CreateProcessDump() to get a dump for the current process.
  MemoryDumpRequestArgs args = {guid, dump_type, level_of_detail};
  delegate->RequestGlobalMemoryDump(args, wrapped_callback);
}

void MemoryDumpManager::RequestGlobalDump(
    MemoryDumpType dump_type,
    MemoryDumpLevelOfDetail level_of_detail) {
  RequestGlobalDump(dump_type, level_of_detail, MemoryDumpCallback());
}

void MemoryDumpManager::CreateProcessDump(const MemoryDumpRequestArgs& args,
                                          const MemoryDumpCallback& callback) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kTraceCategory, "ProcessMemoryDump",
                                    TRACE_ID_MANGLE(args.dump_guid));

  scoped_ptr<ProcessMemoryDumpAsyncState> pmd_async_state;
  {
    AutoLock lock(lock_);
    pmd_async_state.reset(
        new ProcessMemoryDumpAsyncState(args, dump_providers_, session_state_,
                                        callback, dump_thread_->task_runner()));
  }

  TRACE_EVENT_WITH_FLOW0(kTraceCategory, "MemoryDumpManager::CreateProcessDump",
                         TRACE_ID_MANGLE(args.dump_guid),
                         TRACE_EVENT_FLAG_FLOW_OUT);

  // Start the thread hop. |dump_providers_| are kept sorted by thread, so
  // ContinueAsyncProcessDump will hop at most once per thread (w.r.t. thread
  // affinity specified by the MemoryDumpProvider(s) in RegisterDumpProvider()).
  ContinueAsyncProcessDump(pmd_async_state.release());
}

// At most one ContinueAsyncProcessDump() can be active at any time for a given
// PMD, regardless of status of the |lock_|. |lock_| is used here purely to
// ensure consistency w.r.t. (un)registrations of |dump_providers_|.
// The linearization of dump providers' OnMemoryDump invocations is achieved by
// means of subsequent PostTask(s).
//
// 1) Prologue:
//   - If this was the last hop, create a trace event, add it to the trace
//     and finalize (invoke callback).
//   - Check if we are on the right thread. If not hop and continue there.
//   - Check if the dump provider is disabled, if so skip the dump.
// 2) Invoke the dump provider's OnMemoryDump() (unless skipped).
// 3) Epilogue:
//   - Unregister the dump provider if it failed too many times consecutively.
//   - Pop() the MDP from the |pending_dump_providers| list, eventually
//     destroying the MDPInfo if that was unregistered in the meantime.
void MemoryDumpManager::ContinueAsyncProcessDump(
    ProcessMemoryDumpAsyncState* owned_pmd_async_state) {
  // Initalizes the ThreadLocalEventBuffer to guarantee that the TRACE_EVENTs
  // in the PostTask below don't end up registering their own dump providers
  // (for discounting trace memory overhead) while holding the |lock_|.
  TraceLog::GetInstance()->InitializeThreadLocalEventBufferIfSupported();

  // In theory |owned_pmd_async_state| should be a scoped_ptr. The only reason
  // why it isn't is because of the corner case logic of |did_post_task| below,
  // which needs to take back the ownership of the |pmd_async_state| when a
  // thread goes away and consequently the PostTask() fails.
  // Unfortunately, PostTask() destroys the scoped_ptr arguments upon failure
  // to prevent accidental leaks. Using a scoped_ptr would prevent us to to
  // skip the hop and move on. Hence the manual naked -> scoped ptr juggling.
  auto pmd_async_state = make_scoped_ptr(owned_pmd_async_state);
  owned_pmd_async_state = nullptr;

  if (pmd_async_state->pending_dump_providers.empty())
    return FinalizeDumpAndAddToTrace(std::move(pmd_async_state));

  // Read MemoryDumpProviderInfo thread safety considerations in
  // memory_dump_manager.h when accessing |mdpinfo| fields.
  MemoryDumpProviderInfo* mdpinfo =
      pmd_async_state->pending_dump_providers.back().get();

  // If the dump provider did not specify a thread affinity, dump on
  // |dump_thread_|. Note that |dump_thread_| might have been Stop()-ed at this
  // point (if tracing was disabled in the meanwhile). In such case the
  // PostTask() below will fail, but |task_runner| should always be non-null.
  SingleThreadTaskRunner* task_runner = mdpinfo->task_runner.get();
  if (!task_runner)
    task_runner = pmd_async_state->dump_thread_task_runner.get();

  bool post_task_failed = false;
  if (!task_runner->BelongsToCurrentThread()) {
    // It's time to hop onto another thread.
    post_task_failed = !task_runner->PostTask(
        FROM_HERE, Bind(&MemoryDumpManager::ContinueAsyncProcessDump,
                        Unretained(this), Unretained(pmd_async_state.get())));
    if (!post_task_failed) {
      // Ownership is tranferred to the next ContinueAsyncProcessDump().
      ignore_result(pmd_async_state.release());
      return;
    }
  }

  // At this point either:
  // - The MDP has a task runner affinity and we are on the right thread.
  // - The MDP has a task runner affinity but the underlying thread is gone,
  //   hence the above |post_task_failed| == true.
  // - The MDP does NOT have a task runner affinity. A locked access is required
  //   to R/W |disabled| (for the UnregisterAndDeleteDumpProviderSoon() case).
  bool should_dump;
  const char* disabled_reason = nullptr;
  {
    AutoLock lock(lock_);
    if (!mdpinfo->disabled) {
      if (mdpinfo->consecutive_failures >= kMaxConsecutiveFailuresCount) {
        mdpinfo->disabled = true;
        disabled_reason =
            "Dump failure, possibly related with sandboxing (crbug.com/461788)."
            " Try --no-sandbox.";
      } else if (post_task_failed) {
        disabled_reason = "The thread it was meant to dump onto is gone.";
        mdpinfo->disabled = true;
      }
    }
    should_dump = !mdpinfo->disabled;
  }

  if (disabled_reason) {
    LOG(ERROR) << "Disabling MemoryDumpProvider \"" << mdpinfo->name << "\". "
               << disabled_reason;
  }

  if (should_dump) {
    // Invoke the dump provider.
    TRACE_EVENT_WITH_FLOW1(kTraceCategory,
                           "MemoryDumpManager::ContinueAsyncProcessDump",
                           TRACE_ID_MANGLE(pmd_async_state->req_args.dump_guid),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "dump_provider.name", mdpinfo->name);

    // Pid of the target process being dumped. Often kNullProcessId (= current
    // process), non-zero when the coordinator process creates dumps on behalf
    // of child processes (see crbug.com/461788).
    ProcessId target_pid = mdpinfo->options.target_pid;
    ProcessMemoryDump* pmd =
        pmd_async_state->GetOrCreateMemoryDumpContainerForProcess(target_pid);
    MemoryDumpArgs args = {pmd_async_state->req_args.level_of_detail};
    bool dump_successful = mdpinfo->dump_provider->OnMemoryDump(args, pmd);
    mdpinfo->consecutive_failures =
        dump_successful ? 0 : mdpinfo->consecutive_failures + 1;
  }  // if (!mdpinfo->disabled)

  pmd_async_state->pending_dump_providers.pop_back();
  ContinueAsyncProcessDump(pmd_async_state.release());
}

// static
void MemoryDumpManager::FinalizeDumpAndAddToTrace(
    scoped_ptr<ProcessMemoryDumpAsyncState> pmd_async_state) {
  DCHECK(pmd_async_state->pending_dump_providers.empty());
  const uint64_t dump_guid = pmd_async_state->req_args.dump_guid;
  if (!pmd_async_state->callback_task_runner->BelongsToCurrentThread()) {
    scoped_refptr<SingleThreadTaskRunner> callback_task_runner =
        pmd_async_state->callback_task_runner;
    callback_task_runner->PostTask(
        FROM_HERE, Bind(&MemoryDumpManager::FinalizeDumpAndAddToTrace,
                        Passed(&pmd_async_state)));
    return;
  }

  TRACE_EVENT_WITH_FLOW0(kTraceCategory,
                         "MemoryDumpManager::FinalizeDumpAndAddToTrace",
                         TRACE_ID_MANGLE(dump_guid), TRACE_EVENT_FLAG_FLOW_IN);

  for (const auto& kv : pmd_async_state->process_dumps) {
    ProcessId pid = kv.first;  // kNullProcessId for the current process.
    ProcessMemoryDump* process_memory_dump = kv.second.get();
    TracedValue* traced_value = new TracedValue();
    scoped_refptr<ConvertableToTraceFormat> event_value(traced_value);
    process_memory_dump->AsValueInto(traced_value);
    traced_value->SetString("level_of_detail",
                            MemoryDumpLevelOfDetailToString(
                                pmd_async_state->req_args.level_of_detail));
    const char* const event_name =
        MemoryDumpTypeToString(pmd_async_state->req_args.dump_type);

    TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
        TRACE_EVENT_PHASE_MEMORY_DUMP,
        TraceLog::GetCategoryGroupEnabled(kTraceCategory), event_name,
        dump_guid, pid, kTraceEventNumArgs, kTraceEventArgNames,
        kTraceEventArgTypes, nullptr /* arg_values */, &event_value,
        TRACE_EVENT_FLAG_HAS_ID);
  }

  if (!pmd_async_state->callback.is_null()) {
    pmd_async_state->callback.Run(dump_guid, true /* success */);
    pmd_async_state->callback.Reset();
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0(kTraceCategory, "ProcessMemoryDump",
                                  TRACE_ID_MANGLE(dump_guid));
}

void MemoryDumpManager::OnTraceLogEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kTraceCategory, &enabled);
  if (!enabled)
    return;

  // Initialize the TraceLog for the current thread. This is to avoid that the
  // TraceLog memory dump provider is registered lazily in the PostTask() below
  // while the |lock_| is taken;
  TraceLog::GetInstance()->InitializeThreadLocalEventBufferIfSupported();

  // Spin-up the thread used to invoke unbound dump providers.
  scoped_ptr<Thread> dump_thread(new Thread("MemoryInfra"));
  if (!dump_thread->Start()) {
    LOG(ERROR) << "Failed to start the memory-infra thread for tracing";
    return;
  }

  AutoLock lock(lock_);

  DCHECK(delegate_);  // At this point we must have a delegate.

  scoped_refptr<StackFrameDeduplicator> stack_frame_deduplicator = nullptr;
  scoped_refptr<TypeNameDeduplicator> type_name_deduplicator = nullptr;

  if (heap_profiling_enabled_) {
    // If heap profiling is enabled, the stack frame deduplicator and type name
    // deduplicator will be in use. Add a metadata events to write the frames
    // and type IDs.
    stack_frame_deduplicator = new StackFrameDeduplicator;
    type_name_deduplicator = new TypeNameDeduplicator;
    TRACE_EVENT_API_ADD_METADATA_EVENT(
        "stackFrames", "stackFrames",
        scoped_refptr<ConvertableToTraceFormat>(stack_frame_deduplicator));
    TRACE_EVENT_API_ADD_METADATA_EVENT(
        "typeNames", "typeNames",
        scoped_refptr<ConvertableToTraceFormat>(type_name_deduplicator));
  }

  DCHECK(!dump_thread_);
  dump_thread_ = std::move(dump_thread);
  session_state_ = new MemoryDumpSessionState(stack_frame_deduplicator,
                                              type_name_deduplicator);

  subtle::NoBarrier_Store(&memory_tracing_enabled_, 1);

  // TODO(primiano): This is a temporary hack to disable periodic memory dumps
  // when running memory benchmarks until telemetry uses TraceConfig to
  // enable/disable periodic dumps. See crbug.com/529184 .
  if (!is_coordinator_ ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          "enable-memory-benchmarking")) {
    return;
  }

  // Enable periodic dumps. At the moment the periodic support is limited to at
  // most one low-detail periodic dump and at most one high-detail periodic
  // dump. If both are specified the high-detail period must be an integer
  // multiple of the low-level one.
  g_periodic_dumps_count = 0;
  const TraceConfig trace_config =
      TraceLog::GetInstance()->GetCurrentTraceConfig();
  const TraceConfig::MemoryDumpConfig& config_list =
      trace_config.memory_dump_config();
  if (config_list.empty())
    return;

  uint32_t min_timer_period_ms = std::numeric_limits<uint32_t>::max();
  uint32_t heavy_dump_period_ms = 0;
  DCHECK_LE(config_list.size(), 2u);
  for (const TraceConfig::MemoryDumpTriggerConfig& config : config_list) {
    DCHECK(config.periodic_interval_ms);
    if (config.level_of_detail == MemoryDumpLevelOfDetail::DETAILED)
      heavy_dump_period_ms = config.periodic_interval_ms;
    min_timer_period_ms =
        std::min(min_timer_period_ms, config.periodic_interval_ms);
  }
  DCHECK_EQ(0u, heavy_dump_period_ms % min_timer_period_ms);
  g_heavy_dumps_rate = heavy_dump_period_ms / min_timer_period_ms;

  periodic_dump_timer_.Start(FROM_HERE,
                             TimeDelta::FromMilliseconds(min_timer_period_ms),
                             base::Bind(&RequestPeriodicGlobalDump));
}

void MemoryDumpManager::OnTraceLogDisabled() {
  subtle::NoBarrier_Store(&memory_tracing_enabled_, 0);
  scoped_ptr<Thread> dump_thread;
  {
    AutoLock lock(lock_);
    dump_thread = std::move(dump_thread_);
    session_state_ = nullptr;
  }

  // Thread stops are blocking and must be performed outside of the |lock_|
  // or will deadlock (e.g., if ContinueAsyncProcessDump() tries to acquire it).
  periodic_dump_timer_.Stop();
  if (dump_thread)
    dump_thread->Stop();
}

uint64_t MemoryDumpManager::GetTracingProcessId() const {
  return delegate_->GetTracingProcessId();
}

MemoryDumpManager::MemoryDumpProviderInfo::MemoryDumpProviderInfo(
    MemoryDumpProvider* dump_provider,
    const char* name,
    const scoped_refptr<SingleThreadTaskRunner>& task_runner,
    const MemoryDumpProvider::Options& options)
    : dump_provider(dump_provider),
      name(name),
      task_runner(task_runner),
      options(options),
      consecutive_failures(0),
      disabled(false) {}

MemoryDumpManager::MemoryDumpProviderInfo::~MemoryDumpProviderInfo() {}

bool MemoryDumpManager::MemoryDumpProviderInfo::Comparator::operator()(
    const scoped_refptr<MemoryDumpManager::MemoryDumpProviderInfo>& a,
    const scoped_refptr<MemoryDumpManager::MemoryDumpProviderInfo>& b) const {
  if (!a || !b)
    return a.get() < b.get();
  // Ensure that unbound providers (task_runner == nullptr) always run last.
  // Rationale: some unbound dump providers are known to be slow, keep them last
  // to avoid skewing timings of the other dump providers.
  return std::tie(a->task_runner, a->dump_provider) >
         std::tie(b->task_runner, b->dump_provider);
}

MemoryDumpManager::ProcessMemoryDumpAsyncState::ProcessMemoryDumpAsyncState(
    MemoryDumpRequestArgs req_args,
    const MemoryDumpProviderInfo::OrderedSet& dump_providers,
    const scoped_refptr<MemoryDumpSessionState>& session_state,
    MemoryDumpCallback callback,
    const scoped_refptr<SingleThreadTaskRunner>& dump_thread_task_runner)
    : req_args(req_args),
      session_state(session_state),
      callback(callback),
      callback_task_runner(MessageLoop::current()->task_runner()),
      dump_thread_task_runner(dump_thread_task_runner) {
  pending_dump_providers.reserve(dump_providers.size());
  pending_dump_providers.assign(dump_providers.rbegin(), dump_providers.rend());
}

MemoryDumpManager::ProcessMemoryDumpAsyncState::~ProcessMemoryDumpAsyncState() {
}

ProcessMemoryDump* MemoryDumpManager::ProcessMemoryDumpAsyncState::
    GetOrCreateMemoryDumpContainerForProcess(ProcessId pid) {
  auto iter = process_dumps.find(pid);
  if (iter == process_dumps.end()) {
    scoped_ptr<ProcessMemoryDump> new_pmd(new ProcessMemoryDump(session_state));
    iter = process_dumps.insert(std::make_pair(pid, std::move(new_pmd))).first;
  }
  return iter->second.get();
}

}  // namespace trace_event
}  // namespace base
