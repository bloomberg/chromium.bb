// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/connection_manager.h"

#include "base/bind.h"
#include "base/json/string_escape.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "components/services/heap_profiling/allocation_tracker.h"
#include "components/services/heap_profiling/json_exporter.h"
#include "components/services/heap_profiling/public/cpp/client.h"
#include "components/services/heap_profiling/receiver_pipe.h"
#include "components/services/heap_profiling/stream_parser.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/zlib/zlib.h"

#if defined(OS_WIN)
#include <io.h>
#endif

namespace heap_profiling {

namespace {
const size_t kMinSizeThreshold = 16 * 1024;
const size_t kMinCountThreshold = 1024;
}  // namespace

ConnectionManager::DumpArgs::DumpArgs() = default;
ConnectionManager::DumpArgs::DumpArgs(DumpArgs&& other) noexcept
    : backtrace_storage_lock(std::move(other.backtrace_storage_lock)) {}
ConnectionManager::DumpArgs::~DumpArgs() = default;

// Tracking information for DumpProcessForTracing(). This struct is
// refcounted since there will be many background thread calls (one for each
// AllocationTracker) and the callback is only issued when each has
// responded.
//
// This class is not threadsafe, its members must only be accessed on the
// I/O thread.
struct ConnectionManager::DumpProcessesForTracingTracking
    : public ConnectionManager::DumpArgs,
      public base::RefCountedThreadSafe<DumpProcessesForTracingTracking> {
  DumpProcessesForTracingTracking() = default;

  // Number of processes we're still waiting on responses for. When this gets
  // to 0, the callback will be issued.
  size_t waiting_responses = 0;

  // Callback to issue when dumps are complete.
  DumpProcessesForTracingCallback callback;

  // Info about the request.
  VmRegions vm_regions;

  // Collects the results.
  std::vector<memory_instrumentation::mojom::SharedBufferWithSizePtr> results;

 private:
  friend class base::RefCountedThreadSafe<DumpProcessesForTracingTracking>;
  virtual ~DumpProcessesForTracingTracking() = default;
};

struct ConnectionManager::Connection {
  Connection(AllocationTracker::CompleteCallback complete_cb,
             BacktraceStorage* backtrace_storage,
             base::ProcessId pid,
             mojom::ProfilingClientPtr client,
             scoped_refptr<ReceiverPipe> p,
             mojom::ProcessType process_type,
             uint32_t sampling_rate,
             mojom::StackMode stack_mode,
             bool stream_samples)
      : thread(base::StringPrintf("Sender %lld thread",
                                  static_cast<long long>(pid))),
        client(std::move(client)),
        pipe(p),
        process_type(process_type),
        stack_mode(stack_mode),
        stream_samples(stream_samples),
        tracker(std::move(complete_cb), backtrace_storage),
        sampling_rate(sampling_rate) {}

  ~Connection() {
    // The parser may outlive this class because it's refcounted, make sure no
    // callbacks are issued.
    parser->DisconnectReceivers();
  }

  bool HeapDumpNeedsVmRegions() {
    return stack_mode == mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES ||
           stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES ||
           stack_mode == mojom::StackMode::MIXED;
  }

  base::Thread thread;

  mojom::ProfilingClientPtr client;
  scoped_refptr<ReceiverPipe> pipe;
  scoped_refptr<StreamParser> parser;
  mojom::ProcessType process_type;
  mojom::StackMode stack_mode;
  bool stream_samples;

  // Danger: This lives on the |thread| member above. The connection manager
  // lives on the I/O thread, so accesses to the variable must be synchronized.
  AllocationTracker tracker;

  // When sampling is enabled, allocations are recorded with probability (size /
  // sampling_rate) when size < sampling_rate. When size >= sampling_rate, the
  // aggregate probability of an allocation being recorded is 1.0, but the math
  // and details are tricky. See
  // https://bugs.chromium.org/p/chromium/issues/detail?id=810748#c4.
  // A |sampling_rate| of 1 is equivalent to recording all allocations.
  uint32_t sampling_rate = 1;
};

ConnectionManager::ConnectionManager()
    : blocking_thread_("Blocking thread"), weak_factory_(this) {
  blocking_thread_.Start();
  metrics_timer_.Start(
      FROM_HERE, base::TimeDelta::FromHours(24),
      base::Bind(&ConnectionManager::ReportMetrics, base::Unretained(this)));
}
ConnectionManager::~ConnectionManager() = default;

void ConnectionManager::OnNewConnection(base::ProcessId pid,
                                        mojom::ProfilingClientPtr client,
                                        mojo::ScopedHandle receiver_pipe_end,
                                        mojom::ProcessType process_type,
                                        mojom::ProfilingParamsPtr params) {
  base::AutoLock lock(connections_lock_);

  // Attempting to start profiling on an already profiled processs should have
  // no effect.
  if (connections_.find(pid) != connections_.end())
    return;

  // It's theoretically possible that we started profiling a process, the
  // profiling was stopped [e.g. by hitting the 10-s timeout], and then we tried
  // to start profiling again. The ProfilingClient will refuse to start again.
  // But the ConnectionManager will not be able to distinguish this
  // never-started ProfilingClient from a brand new ProfilingClient that happens
  // to share the same pid. This is a rare condition which should only happen
  // when the user is attempting to manually start profiling for processes, so
  // we ignore this edge case.

  scoped_refptr<ReceiverPipe> new_pipe = base::MakeRefCounted<ReceiverPipe>(
      mojo::UnwrapPlatformHandle(std::move(receiver_pipe_end)));

  // The allocation tracker will call this on a background thread, so thunk
  // back to the current thread with weak pointers.
  AllocationTracker::CompleteCallback complete_cb = base::BindOnce(
      &ConnectionManager::OnConnectionCompleteThunk,
      base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr(), pid);

  auto connection = std::make_unique<Connection>(
      std::move(complete_cb), &backtrace_storage_, pid, std::move(client),
      new_pipe, process_type, params->sampling_rate, params->stack_mode,
      params->stream_samples);

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  connection->thread.StartWithOptions(options);

  connection->parser = base::MakeRefCounted<StreamParser>(&connection->tracker);
  new_pipe->SetReceiver(connection->thread.task_runner(), connection->parser);

  connection->thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ReceiverPipe::StartReadingOnIOThread, new_pipe));

  // Request the client start sending us data.
  connection->client->StartProfiling(std::move(params));

  connections_[pid] = std::move(connection);  // Transfers ownership.
}

std::vector<base::ProcessId> ConnectionManager::GetConnectionPids() {
  base::AutoLock lock(connections_lock_);
  std::vector<base::ProcessId> results;
  results.reserve(connections_.size());
  for (const auto& pair : connections_)
    results.push_back(pair.first);
  return results;
}

std::vector<base::ProcessId>
ConnectionManager::GetConnectionPidsThatNeedVmRegions() {
  base::AutoLock lock(connections_lock_);
  std::vector<base::ProcessId> results;
  results.reserve(connections_.size());
  for (const auto& pair : connections_) {
    if (pair.second->HeapDumpNeedsVmRegions())
      results.push_back(pair.first);
  }
  return results;
}

void ConnectionManager::OnConnectionComplete(base::ProcessId pid) {
  base::AutoLock lock(connections_lock_);
  auto found = connections_.find(pid);
  CHECK(found != connections_.end());
  connections_.erase(found);
}

void ConnectionManager::ReportMetrics() {
  base::AutoLock lock(connections_lock_);
  for (auto& pair : connections_) {
    UMA_HISTOGRAM_ENUMERATION("OutOfProcessHeapProfiling.ProfiledProcess.Type",
                              pair.second->process_type,
                              static_cast<int>(mojom::ProcessType::LAST) + 1);
  }
}

// static
void ConnectionManager::OnConnectionCompleteThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<ConnectionManager> connection_manager,
    base::ProcessId pid) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&ConnectionManager::OnConnectionComplete,
                                       connection_manager, pid));
}

void ConnectionManager::DumpProcessesForTracing(
    bool strip_path_from_mapped_files,
    DumpProcessesForTracingCallback callback,
    VmRegions vm_regions) {
  base::AutoLock lock(connections_lock_);

  // Early out if there are no connections.
  if (connections_.empty()) {
    std::move(callback).Run(
        std::vector<memory_instrumentation::mojom::SharedBufferWithSizePtr>());
    return;
  }

  auto tracking = base::MakeRefCounted<DumpProcessesForTracingTracking>();
  tracking->backtrace_storage_lock =
      BacktraceStorage::Lock(&backtrace_storage_);
  tracking->waiting_responses = connections_.size();
  tracking->callback = std::move(callback);
  tracking->vm_regions = std::move(vm_regions);
  tracking->results.reserve(connections_.size());

  for (auto& it : connections_) {
    base::ProcessId pid = it.first;
    Connection* connection = it.second.get();
    if (!connection->stream_samples) {
      connection->client->RetrieveHeapProfile(base::BindOnce(
          &ConnectionManager::HeapProfileRetrieved, weak_factory_.GetWeakPtr(),
          tracking, pid, connection->process_type, strip_path_from_mapped_files,
          connection->sampling_rate));
      continue;
    }
    int barrier_id = next_barrier_id_++;

    // Register for callback before requesting the dump so we don't race for the
    // signal. The callback will be issued on the allocation tracker thread so
    // need to thunk back to the I/O thread.
    connection->tracker.SnapshotOnBarrier(
        barrier_id, base::ThreadTaskRunnerHandle::Get(),
        base::BindOnce(&ConnectionManager::DoDumpOneProcessForTracing,
                       weak_factory_.GetWeakPtr(), tracking, pid,
                       connection->process_type, strip_path_from_mapped_files,
                       connection->sampling_rate));
    connection->client->FlushMemlogPipe(barrier_id);
  }
}

void ConnectionManager::HeapProfileRetrieved(
    scoped_refptr<DumpProcessesForTracingTracking> tracking,
    base::ProcessId pid,
    mojom::ProcessType process_type,
    bool strip_path_from_mapped_files,
    uint32_t sampling_rate,
    mojom::HeapProfilePtr profile) {
  AllocationCountMap counts;
  AllocationTracker::ContextMap context_map;
  AllocationTracker::AddressToStringMap string_map;
  BacktraceStorage backtrace_storage;
  BacktraceStorage::Lock backtrace_storage_lock(&backtrace_storage);

  bool success = true;
  for (const mojom::HeapProfileSamplePtr& sample : profile->samples) {
    int context_id = 0;
    if (sample->context_id) {
      auto it = profile->strings.find(sample->context_id);
      if (it == profile->strings.end()) {
        success = false;
        break;
      }
      const std::string& context = it->second;
      // Escape the strings early, to simplify exporting a heap dump.
      std::string escaped_context;
      base::EscapeJSONString(context, false /* put_in_quotes */,
                             &escaped_context);
      context_id = context_map
                       .emplace(std::move(escaped_context),
                                static_cast<int>(context_map.size() + 1))
                       .first->second;
    }
    const Backtrace* backtrace = backtrace_storage.Insert(
        std::vector<Address>(sample->stack.begin(), sample->stack.end()));
    AllocatorType allocator = static_cast<AllocatorType>(sample->allocator);
    if (allocator >= AllocatorType::kCount) {
      success = false;
      break;
    }
    AllocationEvent alloc(allocator, Address(0), sample->size, backtrace,
                          context_id);
    ++counts[alloc];
  }

  for (const auto& str : profile->strings) {
    std::string quoted_string;
    // Escape the strings before saving them, to simplify exporting a heap dump.
    base::EscapeJSONString(str.second, false /* put_in_quotes */,
                           &quoted_string);
    string_map.emplace(str.first, std::move(quoted_string));
  }

  DCHECK(success);
  DoDumpOneProcessForTracing(tracking, pid, process_type,
                             strip_path_from_mapped_files, sampling_rate,
                             success, std::move(counts), std::move(context_map),
                             std::move(string_map));
}

void ConnectionManager::DoDumpOneProcessForTracing(
    scoped_refptr<DumpProcessesForTracingTracking> tracking,
    base::ProcessId pid,
    mojom::ProcessType process_type,
    bool strip_path_from_mapped_files,
    uint32_t sampling_rate,
    bool success,
    AllocationCountMap counts,
    AllocationTracker::ContextMap context,
    AllocationTracker::AddressToStringMap mapped_strings) {
  // All code paths through here must issue the callback when waiting_responses
  // is 0 or the browser will wait forever for the dump.
  DCHECK(tracking->waiting_responses > 0);

  if (!success) {
    tracking->waiting_responses--;
    if (tracking->waiting_responses == 0)
      std::move(tracking->callback).Run(std::move(tracking->results));
    return;
  }

  CHECK(tracking->backtrace_storage_lock.IsLocked());
  ExportParams params;
  params.allocs = std::move(counts);

  auto it = tracking->vm_regions.find(pid);
  if (it != tracking->vm_regions.end()) {
    params.maps = std::move(it->second);
  }

  params.context_map = std::move(context);
  params.mapped_strings = std::move(mapped_strings);
  params.process_type = process_type;
  params.min_size_threshold = kMinSizeThreshold;
  params.min_count_threshold = kMinCountThreshold;
  params.strip_path_from_mapped_files = strip_path_from_mapped_files;
  params.next_id = next_id_;
  params.sampling_rate = sampling_rate;

  std::ostringstream oss;
  ExportMemoryMapsAndV2StackTraceToJSON(&params, oss);
  std::string reply = oss.str();
  size_t reply_size = reply.size();

  next_id_ = params.next_id;

  using FinishedCallback =
      base::OnceCallback<void(mojo::ScopedSharedBufferHandle)>;
  FinishedCallback finished_callback = base::BindOnce(
      [](std::string reply, base::ProcessId pid,
         scoped_refptr<DumpProcessesForTracingTracking> tracking,
         mojo::ScopedSharedBufferHandle buffer) {
        if (!buffer.is_valid()) {
          DLOG(ERROR) << "Could not create Mojo shared buffer";
        } else {
          mojo::ScopedSharedBufferMapping mapping = buffer->Map(reply.size());
          if (!mapping) {
            DLOG(ERROR) << "Could not map Mojo shared buffer";
          } else {
            memcpy(mapping.get(), reply.c_str(), reply.size());

            memory_instrumentation::mojom::SharedBufferWithSizePtr result =
                memory_instrumentation::mojom::SharedBufferWithSize::New();
            result->buffer = std::move(buffer);
            result->size = reply.size();
            result->pid = pid;
            tracking->results.push_back(std::move(result));
          }
        }

        // When all responses complete, issue done callback.
        tracking->waiting_responses--;
        if (tracking->waiting_responses == 0)
          std::move(tracking->callback).Run(std::move(tracking->results));
      },
      std::move(reply), pid, tracking);

  blocking_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](size_t size,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        FinishedCallback callback) {
                       // This call will send a synchronous IPC to the browser
                       // process.
                       mojo::ScopedSharedBufferHandle buffer =
                           mojo::SharedBufferHandle::Create(size);
                       task_runner->PostTask(FROM_HERE,
                                             base::BindOnce(std::move(callback),
                                                            std::move(buffer)));
                     },
                     reply_size, base::ThreadTaskRunnerHandle::Get(),
                     std::move(finished_callback)));
}

}  // namespace heap_profiling
