// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_test_driver.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/heap_profiler_event_filter.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/profiling/memlog_allocator_shim.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/service_manager_connection.h"

namespace profiling {

namespace {

const char kTestCategory[] = "kTestCategory";
const char kMallocEvent[] = "kMallocEvent";
const char kMallocTypeTag[] = "kMallocTypeTag";
const char kPAEvent[] = "kPAEvent";
const char kVariadicEvent[] = "kVariadicEvent";
const char kThreadName[] = "kThreadName";

// Note: When we test sampling with |sample_everything| = true, we set the
// sampling interval to 2. It's important that all allocations made in this file
// have size >> 2, so that the probability that they are sampled is
// exponentially close to 1.
//
// Make some specific allocations in Browser to do a deeper test of the
// allocation tracking.
constexpr int kMallocAllocSize = 7907;
constexpr int kMallocAllocCount = 157;

constexpr int kVariadicAllocCount = 157;

// The sample rate should not affect the sampled allocations. Intentionally
// choose an odd number.
constexpr int kSampleRate = 7777;
constexpr int kSamplingAllocSize = 100;
constexpr int kSamplingAllocCount = 10000;
const char kSamplingAllocTypeName[] = "kSamplingAllocTypeName";

// Test fixed-size partition alloc. The size must be aligned to system pointer
// size.
constexpr int kPartitionAllocSize = 8 * 23;
constexpr int kPartitionAllocCount = 107;
static const char* kPartitionAllocTypeName = "kPartitionAllocTypeName";

// Whether at least 1 renderer exists, and all renderers are being profiled.
bool RenderersAreBeingProfiled(
    const std::vector<base::ProcessId>& profiled_pids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  size_t renderer_count = 0;
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    base::ProcessId pid = base::GetProcId(iter.GetCurrentValue()->GetHandle());
    if (std::find(profiled_pids.begin(), profiled_pids.end(), pid) ==
        profiled_pids.end()) {
      return false;
    }
    ++renderer_count;
  }

  return renderer_count != 0;
}

// On success, populates |pid|.
int NumProcessesWithName(base::Value* dump_json, std::string name, int* pid) {
  int num_processes = 0;
  base::Value* events = dump_json->FindKey("traceEvents");
  for (const base::Value& event : events->GetList()) {
    const base::Value* found_name =
        event.FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_name)
      continue;
    if (found_name->GetString() != "process_name")
      continue;
    const base::Value* found_args =
        event.FindKeyOfType("args", base::Value::Type::DICTIONARY);
    if (!found_args)
      continue;
    const base::Value* found_process_name =
        found_args->FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_process_name)
      continue;
    if (found_process_name->GetString() != name)
      continue;

    if (pid) {
      const base::Value* found_pid =
          event.FindKeyOfType("pid", base::Value::Type::INTEGER);
      if (!found_pid) {
        LOG(ERROR) << "Process missing pid.";
        return 0;
      }
      *pid = found_pid->GetInt();
    }

    ++num_processes;
  }
  return num_processes;
}

base::Value* FindArgDump(base::ProcessId pid,
                         base::Value* dump_json,
                         const char* arg) {
  base::Value* events = dump_json->FindKey("traceEvents");
  base::Value* dumps = nullptr;
  base::Value* heaps_v2 = nullptr;
  for (base::Value& event : events->GetList()) {
    const base::Value* found_name =
        event.FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_name)
      continue;
    if (found_name->GetString() != "periodic_interval")
      continue;
    const base::Value* found_pid =
        event.FindKeyOfType("pid", base::Value::Type::INTEGER);
    if (!found_pid)
      continue;
    if (static_cast<base::ProcessId>(found_pid->GetInt()) != pid)
      continue;
    dumps = &event;
    heaps_v2 = dumps->FindPath({"args", "dumps", arg});
    if (heaps_v2)
      return heaps_v2;
  }
  return nullptr;
}

constexpr uint64_t kNullParent = std::numeric_limits<int>::max();
struct Node {
  int name_id;
  std::string name;
  int parent_id = kNullParent;
};
using NodeMap = std::unordered_map<uint64_t, Node>;

// Parses maps.nodes and maps.strings. Returns |true| on success.
bool ParseNodes(base::Value* heaps_v2, NodeMap* output) {
  base::Value* nodes = heaps_v2->FindPath({"maps", "nodes"});
  for (const base::Value& node_value : nodes->GetList()) {
    const base::Value* id = node_value.FindKey("id");
    const base::Value* name_sid = node_value.FindKey("name_sid");
    if (!id || !name_sid) {
      LOG(ERROR) << "Node missing id or name_sid field";
      return false;
    }

    Node node;
    node.name_id = name_sid->GetInt();

    const base::Value* parent_id = node_value.FindKey("parent");
    if (parent_id) {
      node.parent_id = parent_id->GetInt();
    }

    (*output)[id->GetInt()] = node;
  }

  base::Value* strings = heaps_v2->FindPath({"maps", "strings"});
  for (const base::Value& string_value : strings->GetList()) {
    const base::Value* id = string_value.FindKey("id");
    const base::Value* string = string_value.FindKey("string");
    if (!id || !string) {
      LOG(ERROR) << "String struct missing id or string field";
      return false;
    }
    for (auto& pair : *output) {
      if (pair.second.name_id == id->GetInt()) {
        pair.second.name = string->GetString();
        break;
      }
    }
  }

  return true;
}

// Parses maps.types and maps.strings. Returns |true| on success.
bool ParseTypes(base::Value* heaps_v2, NodeMap* output) {
  base::Value* types = heaps_v2->FindPath({"maps", "types"});
  for (const base::Value& type_value : types->GetList()) {
    const base::Value* id = type_value.FindKey("id");
    const base::Value* name_sid = type_value.FindKey("name_sid");
    if (!id || !name_sid) {
      LOG(ERROR) << "Node missing id or name_sid field";
      return false;
    }

    Node node;
    node.name_id = name_sid->GetInt();
    (*output)[id->GetInt()] = node;
  }

  base::Value* strings = heaps_v2->FindPath({"maps", "strings"});
  for (const base::Value& string_value : strings->GetList()) {
    const base::Value* id = string_value.FindKey("id");
    const base::Value* string = string_value.FindKey("string");
    if (!id || !string) {
      LOG(ERROR) << "String struct missing id or string field";
      return false;
    }
    for (auto& pair : *output) {
      if (pair.second.name_id == id->GetInt()) {
        pair.second.name = string->GetString();
        break;
      }
    }
  }

  return true;
}

// Verify expectations are present in heap dump.
bool ValidateDump(base::Value* heaps_v2,
                  int expected_alloc_size,
                  int expected_alloc_count,
                  const char* allocator_name,
                  const char* type_name,
                  const std::string& frame_name,
                  const std::string& thread_name) {
  base::Value* sizes =
      heaps_v2->FindPath({"allocators", allocator_name, "sizes"});
  if (!sizes) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name
               << ".sizes' in heaps v2";
    return false;
  }

  const base::Value::ListStorage& sizes_list = sizes->GetList();
  if (sizes_list.empty()) {
    LOG(ERROR) << "'allocators." << allocator_name
               << ".sizes' is an empty list";
    return false;
  }

  base::Value* counts =
      heaps_v2->FindPath({"allocators", allocator_name, "counts"});
  if (!counts) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name
               << ".counts' in heaps v2";
    return false;
  }

  const base::Value::ListStorage& counts_list = counts->GetList();
  if (sizes_list.size() != counts_list.size()) {
    LOG(ERROR)
        << "'allocators." << allocator_name
        << ".sizes' does not have the same number of elements as *.counts";
    return false;
  }

  base::Value* types =
      heaps_v2->FindPath({"allocators", allocator_name, "types"});
  if (!types) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name
               << ".types' in heaps v2";
    return false;
  }

  const base::Value::ListStorage& types_list = types->GetList();
  if (types_list.empty()) {
    LOG(ERROR) << "'allocators." << allocator_name
               << ".types' is an empty list";
    return false;
  }

  if (sizes_list.size() != types_list.size()) {
    LOG(ERROR)
        << "'allocators." << allocator_name
        << ".types' does not have the same number of elements as *.sizes";
    return false;
  }

  base::Value* nodes =
      heaps_v2->FindPath({"allocators", allocator_name, "nodes"});
  if (!nodes) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name
               << ".nodes' in heaps v2";
    return false;
  }

  const base::Value::ListStorage& nodes_list = nodes->GetList();
  if (sizes_list.size() != nodes_list.size()) {
    LOG(ERROR)
        << "'allocators." << allocator_name
        << ".sizes' does not have the same number of elements as *.nodes";
    return false;
  }

  bool found_browser_alloc = false;
  size_t browser_alloc_index = 0;
  for (size_t i = 0; i < sizes_list.size(); i++) {
    if (counts_list[i].GetInt() == expected_alloc_count &&
        sizes_list[i].GetInt() != expected_alloc_size) {
      LOG(WARNING) << "Allocation candidate (size:" << sizes_list[i].GetInt()
                   << " count:" << counts_list[i].GetInt() << ")";
    }
    if (sizes_list[i].GetInt() == expected_alloc_size &&
        counts_list[i].GetInt() == expected_alloc_count) {
      browser_alloc_index = i;
      found_browser_alloc = true;
      break;
    }
  }

  if (!found_browser_alloc) {
    LOG(ERROR) << "Failed to find an allocation of the "
                  "appropriate size. Did the send buffer "
                  "not flush? (size: "
               << expected_alloc_size << " count:" << expected_alloc_count
               << ")";
    return false;
  }

  // Find the type, if an expectation was passed in.
  if (type_name) {
    NodeMap node_map;
    if (!ParseTypes(heaps_v2, &node_map)) {
      LOG(ERROR) << "Failed to parse type and string structs";
      return false;
    }

    int type = types_list[browser_alloc_index].GetInt();
    auto it = node_map.find(type);
    if (it == node_map.end()) {
      LOG(ERROR) << "Failed to look up type.";
      return false;
    }
    if (it->second.name != type_name) {
      LOG(ERROR) << "actual name: " << it->second.name
                 << " expected name: " << type_name;
      return false;
    }
  }

  // Check that the frame has the right name.
  if (!frame_name.empty()) {
    NodeMap node_map;
    if (!ParseNodes(heaps_v2, &node_map)) {
      LOG(ERROR) << "Failed to parse node and string structs";
      return false;
    }

    int node_id = nodes_list[browser_alloc_index].GetInt();
    auto it = node_map.find(node_id);

    if (it == node_map.end()) {
      LOG(ERROR) << "Failed to find frame for node with id: " << node_id;
      return false;
    }

    if (it->second.name != frame_name) {
      LOG(ERROR) << "Wrong name: " << it->second.name
                 << " for frame with expected name: " << frame_name;
      return false;
    }
  }

  // Check that the thread [top frame] has the right name.
  if (!thread_name.empty()) {
    NodeMap node_map;
    if (!ParseNodes(heaps_v2, &node_map)) {
      LOG(ERROR) << "Failed to parse node and string structs";
      return false;
    }

    int node_id = nodes_list[browser_alloc_index].GetInt();
    auto it = node_map.find(node_id);
    while (true) {
      if (it == node_map.end() || it->second.parent_id == kNullParent)
        break;
      it = node_map.find(it->second.parent_id);
    }

    if (it == node_map.end()) {
      LOG(ERROR) << "Failed to find root for node with id: " << node_id;
      return false;
    }

    if (it->second.name != thread_name) {
      LOG(ERROR) << "Wrong name: " << it->second.name
                 << " for thread with expected name: " << thread_name;
      return false;
    }
  }

  return true;
}

// |expected_size| of 0 means no expectation.
bool GetAllocatorSubarray(base::Value* heaps_v2,
                          const char* allocator_name,
                          const char* subarray_name,
                          size_t expected_size,
                          const base::Value::ListStorage** output) {
  base::Value* subarray =
      heaps_v2->FindPath({"allocators", allocator_name, subarray_name});
  if (!subarray) {
    LOG(ERROR) << "Failed to find path: 'allocators." << allocator_name << "."
               << subarray_name << "' in heaps v2";
    return false;
  }

  const base::Value::ListStorage& subarray_list = subarray->GetList();
  if (expected_size && subarray_list.size() != expected_size) {
    LOG(ERROR) << subarray_name << " has wrong size";
    return false;
  }

  *output = &subarray_list;
  return true;
}

bool ValidateSamplingAllocations(base::Value* heaps_v2,
                                 const char* allocator_name,
                                 int approximate_size,
                                 int approximate_count,
                                 const char* type_name) {
  // Maps type ids to strings.
  NodeMap type_map;
  if (!ParseTypes(heaps_v2, &type_map))
    return false;

  bool found = false;
  int id_of_type = 0;
  for (auto& pair : type_map) {
    if (pair.second.name == type_name) {
      id_of_type = pair.first;
      found = true;
    }
  }

  if (!found) {
    LOG(ERROR) << "Failed to find type with name: " << type_name;
    return false;
  }

  // Find the type with the appropriate id.
  const base::Value::ListStorage* types_list;
  if (!GetAllocatorSubarray(heaps_v2, allocator_name, "types", 0,
                            &types_list)) {
    return false;
  }

  found = false;
  size_t index = 0;
  for (size_t i = 0; i < types_list->size(); ++i) {
    if ((*types_list)[i].GetInt() == id_of_type) {
      index = i;
      found = true;
      break;
    }
  }

  if (!found) {
    LOG(ERROR) << "Failed to find type with correct sid";
    return false;
  }

  // Look up the size.
  const base::Value::ListStorage* sizes;
  if (!GetAllocatorSubarray(heaps_v2, allocator_name, "sizes",
                            types_list->size(), &sizes)) {
    return false;
  }

  if ((*sizes)[index].GetInt() < approximate_size / 2 ||
      (*sizes)[index].GetInt() > approximate_size * 2) {
    LOG(ERROR) << "sampling size " << (*sizes)[index].GetInt()
               << " was not within a factor of 2 of expected size "
               << approximate_size;
    return false;
  }

  // Look up the count.
  const base::Value::ListStorage* counts;
  if (!GetAllocatorSubarray(heaps_v2, allocator_name, "counts",
                            types_list->size(), &counts)) {
    return false;
  }

  if ((*counts)[index].GetInt() < approximate_count / 2 ||
      (*counts)[index].GetInt() > approximate_count * 2) {
    LOG(ERROR) << "sampling size " << (*counts)[index].GetInt()
               << " was not within a factor of 2 of expected count "
               << approximate_count;
    return false;
  }
  return true;
}

bool ValidateProcessMmaps(base::Value* process_mmaps,
                          bool should_have_contents) {
  base::Value* vm_regions = process_mmaps->FindKey("vm_regions");
  size_t count = vm_regions->GetList().size();
  if (should_have_contents) {
    if (count == 0) {
      LOG(ERROR) << "vm_regions should have contents, but doesn't";
      return false;
    }
  } else {
    if (count != 0) {
      LOG(ERROR) << "vm_regions should be empty, but has contents";
      return false;
    }
  }
  return true;
}

}  // namespace

ProfilingTestDriver::ProfilingTestDriver()
    : wait_for_ui_thread_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  partition_allocator_.init();
}
ProfilingTestDriver::~ProfilingTestDriver() {}

bool ProfilingTestDriver::RunTest(const Options& options) {
  options_ = options;

  running_on_ui_thread_ =
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI);

  // The only thing to test for Mode::kNone is that profiling hasn't started.
  if (options_.mode == ProfilingProcessHost::Mode::kNone) {
    if (ProfilingProcessHost::has_started()) {
      LOG(ERROR) << "Profiling should not have started";
      return false;
    }
    return true;
  }

  if (running_on_ui_thread_) {
    if (!CheckOrStartProfiling())
      return false;
    if (ShouldProfileRenderer())
      WaitForProfilingToStartForAllRenderersUIThread();
    if (ShouldProfileBrowser())
      MakeTestAllocations();
    CollectResults(true);
  } else {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ProfilingTestDriver::CheckOrStartProfilingOnUIThreadAndSignal,
            base::Unretained(this)));
    wait_for_ui_thread_.Wait();
    if (!initialization_success_)
      return false;
    if (ShouldProfileRenderer()) {
      // On Android, there is a warm-up renderer that is sometimes started
      // before the ProfilingProcessHost can be started. This renderer will
      // therefore not be profiled, even if Chrome is started with the
      // --memlog=all-renderers switch.
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&ProfilingProcessHost::StartProfilingRenderersForTesting,
                     base::Unretained(ProfilingProcessHost::GetInstance())));

      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(
              &ProfilingTestDriver::
                  WaitForProfilingToStartForAllRenderersUIThreadAndSignal,
              base::Unretained(this)));
      wait_for_ui_thread_.Wait();
    }
    if (ShouldProfileBrowser()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&ProfilingTestDriver::MakeTestAllocations,
                     base::Unretained(this)));
    }
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&ProfilingTestDriver::CollectResults, base::Unretained(this),
                   false));
    wait_for_ui_thread_.Wait();
  }

  std::unique_ptr<base::Value> dump_json =
      base::JSONReader::Read(serialized_trace_);
  if (!dump_json) {
    LOG(ERROR) << "Failed to deserialize trace.";
    return false;
  }

  if (!ValidateBrowserAllocations(dump_json.get())) {
    LOG(ERROR) << "Failed to validate browser allocations";
    return false;
  }

  if (!ValidateRendererAllocations(dump_json.get())) {
    LOG(ERROR) << "Failed to validate renderer allocations";
    return false;
  }

  return true;
}

void ProfilingTestDriver::CheckOrStartProfilingOnUIThreadAndSignal() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  initialization_success_ = CheckOrStartProfiling();

  // If the flag is true, then the WaitableEvent will be signaled after
  // profiling has started.
  if (!wait_for_profiling_to_start_)
    wait_for_ui_thread_.Signal();
}

bool ProfilingTestDriver::CheckOrStartProfiling() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (options_.profiling_already_started) {
    if (ProfilingProcessHost::has_started())
      return true;
    LOG(ERROR) << "Profiling should have been started, but wasn't";
    return false;
  }

  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  if (!connection) {
    LOG(ERROR) << "A ServiceManagerConnection was not available for the "
                  "current process.";
    return false;
  }

  // When this is not-null, initialization should wait for the QuitClosure to be
  // called.
  std::unique_ptr<base::RunLoop> run_loop;

  if (ShouldProfileBrowser()) {
    if (running_on_ui_thread_) {
      run_loop.reset(new base::RunLoop);
      profiling::SetOnInitAllocatorShimCallbackForTesting(
          run_loop->QuitClosure(), base::ThreadTaskRunnerHandle::Get());
    } else {
      wait_for_profiling_to_start_ = true;
      profiling::SetOnInitAllocatorShimCallbackForTesting(
          base::Bind(&base::WaitableEvent::Signal,
                     base::Unretained(&wait_for_ui_thread_)),
          base::ThreadTaskRunnerHandle::Get());
    }
  }

  ProfilingProcessHost::Start(connection, options_.mode, options_.stack_mode,
                              options_.should_sample,
                              options_.sample_everything ? 2 : kSampleRate);
  ProfilingProcessHost::GetInstance()->SetKeepSmallAllocations(true);

  if (run_loop)
    run_loop->Run();

  return true;
}

void ProfilingTestDriver::MakeTestAllocations() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::PlatformThread::SetName(kThreadName);

  // In sampling mode, only sampling allocations are relevant.
  if (!IsRecordingAllAllocations()) {
    leaks_.reserve(kSamplingAllocCount);
    for (int i = 0; i < kSamplingAllocCount; ++i) {
      leaks_.push_back(static_cast<char*>(partition_allocator_.root()->Alloc(
          kSamplingAllocSize, kSamplingAllocTypeName)));
    }
    return;
  }

  leaks_.reserve(2 * kMallocAllocCount + 1 + kPartitionAllocSize);

  {
    TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION event(kMallocTypeTag);
    TRACE_EVENT0(kTestCategory, kMallocEvent);

    for (int i = 0; i < kMallocAllocCount; ++i) {
      leaks_.push_back(new char[kMallocAllocSize]);
    }
  }

  {
    TRACE_EVENT0(kTestCategory, kPAEvent);

    for (int i = 0; i < kPartitionAllocCount; ++i) {
      leaks_.push_back(static_cast<char*>(partition_allocator_.root()->Alloc(
          kPartitionAllocSize, kPartitionAllocTypeName)));
    }
  }

  {
    TRACE_EVENT0(kTestCategory, kVariadicEvent);

    for (int i = 0; i < kVariadicAllocCount; ++i) {
      leaks_.push_back(new char[i + 8000]);  // Variadic allocation.
      total_variadic_allocations_ += i + 8000;
    }
  }

  // // Navigate around to force allocations in the renderer.
  // ASSERT_TRUE(embedded_test_server()->Start());
  // ui_test_utils::NavigateToURL(
  //     browser(), embedded_test_server()->GetURL("/english_page.html"));
  // // Vive la France!
  // ui_test_utils::NavigateToURL(
  //     browser(), embedded_test_server()->GetURL("/french_page.html"));
}

void ProfilingTestDriver::CollectResults(bool synchronous) {
  base::Closure finish_tracing_closure;
  std::unique_ptr<base::RunLoop> run_loop;

  if (synchronous) {
    run_loop.reset(new base::RunLoop);
    finish_tracing_closure = run_loop->QuitClosure();
  } else {
    finish_tracing_closure = base::Bind(&base::WaitableEvent::Signal,
                                        base::Unretained(&wait_for_ui_thread_));
  }

  profiling::ProfilingProcessHost::GetInstance()->RequestTraceWithHeapDump(
      base::Bind(&ProfilingTestDriver::TraceFinished, base::Unretained(this),
                 std::move(finish_tracing_closure)),
      false /* strip_path_from_mapped_files */);

  if (synchronous)
    run_loop->Run();
}

void ProfilingTestDriver::TraceFinished(base::Closure closure,
                                        bool success,
                                        std::string trace_json) {
  serialized_trace_.swap(trace_json);
  std::move(closure).Run();
}

bool ProfilingTestDriver::ValidateBrowserAllocations(base::Value* dump_json) {
  base::Value* heaps_v2 =
      FindArgDump(base::Process::Current().Pid(), dump_json, "heaps_v2");

  if (options_.mode != ProfilingProcessHost::Mode::kAll &&
      options_.mode != ProfilingProcessHost::Mode::kBrowser &&
      options_.mode != ProfilingProcessHost::Mode::kMinimal) {
    if (heaps_v2) {
      LOG(ERROR) << "There should be no heap dump for the browser.";
      return false;
    }
    return true;
  }

  if (!heaps_v2) {
    LOG(ERROR) << "Browser heap dump missing.";
    return false;
  }

  bool result = false;

  bool should_validate_dumps = true;
#if defined(OS_ANDROID)
  // TODO(ajwong): This step fails on Nexus 5X devices running kit-kat. It works
  // on Nexus 5X devices running oreo. The problem is that all allocations have
  // the same [an effectively empty] backtrace and get glommed together. More
  // investigation is necessary. For now, I'm turning this off for Android.
  // https://crbug.com/786450.
  if (!HasPseudoFrames())
    should_validate_dumps = false;
#endif

  std::string thread_name = ShouldIncludeNativeThreadNames() ? kThreadName : "";

  if (IsRecordingAllAllocations()) {
    if (should_validate_dumps) {
      result = ValidateDump(heaps_v2, kMallocAllocSize * kMallocAllocCount,
                            kMallocAllocCount, "malloc",
                            HasPseudoFrames() ? kMallocTypeTag : nullptr,
                            HasPseudoFrames() ? kMallocEvent : "", thread_name);
      if (!result) {
        LOG(ERROR) << "Failed to validate malloc fixed allocations";
        return false;
      }

      result = ValidateDump(
          heaps_v2, total_variadic_allocations_, kVariadicAllocCount, "malloc",
          nullptr, HasPseudoFrames() ? kVariadicEvent : "", thread_name);
      if (!result) {
        LOG(ERROR) << "Failed to validate malloc variadic allocations";
        return false;
      }
    }

    // TODO(ajwong): Like malloc, all Partition-Alloc allocations get glommed
    // together for some Android device/OS configurations. However, since there
    // is only one place that uses partition alloc in the browser process [this
    // test], the count is still valid. This should still be made more robust by
    // fixing backtrace. https://crbug.com/786450.
    result = ValidateDump(heaps_v2, kPartitionAllocSize * kPartitionAllocCount,
                          kPartitionAllocCount, "partition_alloc",
                          kPartitionAllocTypeName,
                          HasPseudoFrames() ? kPAEvent : "", thread_name);
    if (!result) {
      LOG(ERROR) << "Failed to validate PA allocations";
      return false;
    }
  } else {
    bool result = ValidateSamplingAllocations(
        heaps_v2, "partition_alloc", kSamplingAllocSize * kSamplingAllocCount,
        kSamplingAllocCount, kSamplingAllocTypeName);
    if (!result) {
      LOG(ERROR) << "Failed to validate sampling allocations";
      return false;
    }
  }

  int process_count = NumProcessesWithName(dump_json, "Browser", nullptr);
  if (process_count != 1) {
    LOG(ERROR) << "Found " << process_count
               << " processes with name: Browser. Expected 1.";
    return false;
  }

  base::Value* process_mmaps =
      FindArgDump(base::Process::Current().Pid(), dump_json, "process_mmaps");
  if (!ValidateProcessMmaps(process_mmaps, HasNativeFrames())) {
    LOG(ERROR) << "Failed to validate browser process mmaps.";
    return false;
  }

  return true;
}

bool ProfilingTestDriver::ValidateRendererAllocations(base::Value* dump_json) {
  int pid;
  bool result = NumProcessesWithName(dump_json, "Renderer", &pid) == 1;
  if (!result) {
    LOG(ERROR) << "Failed to find process with name Renderer";
    return false;
  }

  base::ProcessId renderer_pid = static_cast<base::ProcessId>(pid);
  base::Value* heaps_v2 = FindArgDump(renderer_pid, dump_json, "heaps_v2");
  if (ShouldProfileRenderer()) {
    if (!heaps_v2) {
      LOG(ERROR) << "Failed to find heaps v2 for renderer";
      return false;
    }

    base::Value* process_mmaps =
        FindArgDump(renderer_pid, dump_json, "process_mmaps");
    if (!ValidateProcessMmaps(process_mmaps, HasNativeFrames())) {
      LOG(ERROR) << "Failed to validate renderer process mmaps.";
      return false;
    }

    // ValidateDump doesn't always succeed for the renderer, since we don't do
    // anything to flush allocations, there are very few allocations recorded
    // by the heap profiler. When we do a heap dump, we prune small
    // allocations...and this can cause all allocations to be pruned.
    // ASSERT_NO_FATAL_FAILURE(ValidateDump(dump_json.get(), 0, 0));
  } else {
    if (heaps_v2) {
      LOG(ERROR) << "There should be no heap dump for the renderer.";
      return false;
    }
  }

  if (options_.mode == ProfilingProcessHost::Mode::kAllRenderers) {
    if (NumProcessesWithName(dump_json, "Renderer", nullptr) == 0) {
      LOG(ERROR) << "There should be at least 1 renderer dump";
      return false;
    }
  } else {
    if (NumProcessesWithName(dump_json, "Renderer", nullptr) == 0) {
      LOG(ERROR) << "There should be more than 1 renderer dump";
      return false;
    }
  }

  return true;
}

bool ProfilingTestDriver::ShouldProfileBrowser() {
  return options_.mode == ProfilingProcessHost::Mode::kAll ||
         options_.mode == ProfilingProcessHost::Mode::kBrowser ||
         options_.mode == ProfilingProcessHost::Mode::kMinimal;
}

bool ProfilingTestDriver::ShouldProfileRenderer() {
  return options_.mode == ProfilingProcessHost::Mode::kAll ||
         options_.mode == ProfilingProcessHost::Mode::kAllRenderers;
}

bool ProfilingTestDriver::ShouldIncludeNativeThreadNames() {
  return options_.stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES;
}

bool ProfilingTestDriver::HasPseudoFrames() {
  return options_.stack_mode == mojom::StackMode::PSEUDO ||
         options_.stack_mode == mojom::StackMode::MIXED;
}

bool ProfilingTestDriver::HasNativeFrames() {
  return options_.stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES ||
         options_.stack_mode == mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES ||
         options_.stack_mode == mojom::StackMode::MIXED;
}

bool ProfilingTestDriver::IsRecordingAllAllocations() {
  return !options_.should_sample || options_.sample_everything;
}

void ProfilingTestDriver::WaitForProfilingToStartForAllRenderersUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  while (true) {
    std::vector<base::ProcessId> profiled_pids;
    base::RunLoop run_loop;
    auto callback = base::BindOnce(
        [](std::vector<base::ProcessId>* results, base::OnceClosure finished,
           std::vector<base::ProcessId> pids) {
          results->swap(pids);
          std::move(finished).Run();
        },
        &profiled_pids, run_loop.QuitClosure());
    profiling::ProfilingProcessHost::GetInstance()->GetProfiledPids(
        std::move(callback));
    run_loop.Run();

    if (RenderersAreBeingProfiled(profiled_pids))
      break;
  }
}

void ProfilingTestDriver::
    WaitForProfilingToStartForAllRenderersUIThreadAndSignal() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  profiling::ProfilingProcessHost::GetInstance()->GetProfiledPids(
      base::BindOnce(&ProfilingTestDriver::
                         WaitForProfilingToStartForAllRenderersUIThreadCallback,
                     base::Unretained(this)));
}

void ProfilingTestDriver::
    WaitForProfilingToStartForAllRenderersUIThreadCallback(
        std::vector<base::ProcessId> results) {
  if (RenderersAreBeingProfiled(results)) {
    wait_for_ui_thread_.Signal();
    return;
  }
  WaitForProfilingToStartForAllRenderersUIThreadAndSignal();
}

}  // namespace profiling
