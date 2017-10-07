// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_connection_manager.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "chrome/common/profiling/memlog_client.h"
#include "chrome/profiling/allocation_tracker.h"
#include "chrome/profiling/json_exporter.h"
#include "chrome/profiling/memlog_receiver_pipe.h"
#include "chrome/profiling/memlog_stream_parser.h"
#include "mojo/public/cpp/system/buffer.h"
#include "third_party/zlib/zlib.h"

#if defined(OS_WIN)
#include <io.h>
#endif

namespace profiling {

namespace {
const size_t kMinSizeThreshold = 16 * 1024;
const size_t kMinCountThreshold = 1024;
}  // namespace

MemlogConnectionManager::DumpArgs::DumpArgs() = default;
MemlogConnectionManager::DumpArgs::DumpArgs(DumpArgs&& other) noexcept
    : backtrace_storage_lock(std::move(other.backtrace_storage_lock)) {}
MemlogConnectionManager::DumpArgs::~DumpArgs() = default;

MemlogConnectionManager::DumpProcessArgs::DumpProcessArgs() = default;
MemlogConnectionManager::DumpProcessArgs::DumpProcessArgs(
    DumpProcessArgs&& other) noexcept
    : DumpArgs(std::move(other)),
      pid(other.pid),
      maps(std::move(other.maps)),
      metadata(std::move(other.metadata)),
      file(std::move(other.file)),
      callback(std::move(other.callback)) {}
MemlogConnectionManager::DumpProcessArgs::~DumpProcessArgs() = default;

// Tracking information for DumpProcessForTracing(). This struct is
// refcounted since there will be many background thread calls (one for each
// AllocationTracker) and the callback is only issued when each has
// responded.
//
// This class is not threadsafe, its members must only be accessed on the
// I/O thread.
struct MemlogConnectionManager::DumpProcessesForTracingTracking
    : public MemlogConnectionManager::DumpArgs,
      public base::RefCountedThreadSafe<DumpProcessesForTracingTracking> {
  DumpProcessesForTracingTracking() = default;

  // Number of processes we're still waiting on responses for. When this gets
  // to 0, the callback will be issued.
  size_t waiting_responses = 0;

  // Callback to issue when dumps are complete.
  mojom::Memlog::DumpProcessesForTracingCallback callback;

  // Info about the request.
  memory_instrumentation::mojom::GlobalMemoryDumpPtr dump;

  // Collects the results.
  std::vector<profiling::mojom::SharedBufferWithSizePtr> results;

 private:
  friend class base::RefCountedThreadSafe<DumpProcessesForTracingTracking>;
  virtual ~DumpProcessesForTracingTracking() = default;
};

struct MemlogConnectionManager::Connection {
  Connection(AllocationTracker::CompleteCallback complete_cb,
             BacktraceStorage* backtrace_storage,
             base::ProcessId pid,
             mojom::MemlogClientPtr client,
             scoped_refptr<MemlogReceiverPipe> p)
      : thread(base::StringPrintf("Sender %lld thread",
                                  static_cast<long long>(pid))),
        client(std::move(client)),
        pipe(p),
        tracker(std::move(complete_cb), backtrace_storage) {}

  ~Connection() {
    // The parser may outlive this class because it's refcounted, make sure no
    // callbacks are issued.
    parser->DisconnectReceivers();
  }

  base::Thread thread;

  mojom::MemlogClientPtr client;
  scoped_refptr<MemlogReceiverPipe> pipe;
  scoped_refptr<MemlogStreamParser> parser;

  // Danger: This lives on the |thread| member above. The connection manager
  // lives on the I/O thread, so accesses to the variable must be synchronized.
  AllocationTracker tracker;
};

MemlogConnectionManager::MemlogConnectionManager() : weak_factory_(this) {}
MemlogConnectionManager::~MemlogConnectionManager() = default;

void MemlogConnectionManager::OnNewConnection(
    base::ProcessId pid,
    mojom::MemlogClientPtr client,
    mojo::edk::ScopedPlatformHandle handle) {
  base::AutoLock lock(connections_lock_);
  DCHECK(connections_.find(pid) == connections_.end());

  scoped_refptr<MemlogReceiverPipe> new_pipe =
      new MemlogReceiverPipe(std::move(handle));

  // The allocation tracker will call this on a background thread, so thunk
  // back to the current thread with weak pointers.
  AllocationTracker::CompleteCallback complete_cb =
      base::BindOnce(&MemlogConnectionManager::OnConnectionCompleteThunk,
                     base::MessageLoop::current()->task_runner(),
                     weak_factory_.GetWeakPtr(), pid);

  auto connection =
      base::MakeUnique<Connection>(std::move(complete_cb), &backtrace_storage_,
                                   pid, std::move(client), new_pipe);
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  connection->thread.StartWithOptions(options);

  connection->parser = new MemlogStreamParser(&connection->tracker);
  new_pipe->SetReceiver(connection->thread.task_runner(), connection->parser);

  connections_[pid] = std::move(connection);

  connections_[pid]->thread.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&MemlogReceiverPipe::StartReadingOnIOThread, new_pipe));
}

void MemlogConnectionManager::OnConnectionComplete(base::ProcessId pid) {
  base::AutoLock lock(connections_lock_);
  auto found = connections_.find(pid);
  CHECK(found != connections_.end());
  connections_.erase(found);
}

// static
void MemlogConnectionManager::OnConnectionCompleteThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<MemlogConnectionManager> connection_manager,
    base::ProcessId pid) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MemlogConnectionManager::OnConnectionComplete,
                                connection_manager, pid));
}

void MemlogConnectionManager::DumpProcess(DumpProcessArgs args) {
  args.backtrace_storage_lock = BacktraceStorage::Lock(&backtrace_storage_);
  SynchronizeOnPid(args.pid,
                   base::BindOnce(&MemlogConnectionManager::DoDumpProcess,
                                  weak_factory_.GetWeakPtr(), std::move(args)));
}

void MemlogConnectionManager::DumpProcessesForTracing(
    mojom::Memlog::DumpProcessesForTracingCallback callback,
    memory_instrumentation::mojom::GlobalMemoryDumpPtr dump) {
  base::AutoLock lock(connections_lock_);

  auto tracking = base::MakeRefCounted<DumpProcessesForTracingTracking>();
  tracking->waiting_responses = connections_.size();
  tracking->callback = std::move(callback);
  tracking->dump = std::move(dump);
  tracking->results.reserve(connections_.size());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::MessageLoop::current()->task_runner();

  for (auto& it : connections_) {
    base::ProcessId pid = it.first;
    Connection* connection = it.second.get();
    int barrier_id = next_barrier_id_++;

    // Register for callback before requesting the dump so we don't race for the
    // signal. The callback will be issued on the allocation tracker thread so
    // need to thunk back to the I/O thread.
    connection->tracker.SnapshotOnBarrier(
        barrier_id, task_runner,
        base::BindOnce(&MemlogConnectionManager::DoDumpOneProcessForTracing,
                       weak_factory_.GetWeakPtr(), tracking, pid));
    connection->client->FlushPipe(barrier_id);
  }
}

void MemlogConnectionManager::SynchronizeOnPid(
    base::ProcessId process_id,
    AllocationTracker::SnapshotCallback callback) {
  base::AutoLock lock(connections_lock_);

  auto task_runner = base::MessageLoop::current()->task_runner();

  auto it = connections_.find(process_id);
  if (it == connections_.end()) {
    DLOG(ERROR) << "No connections found for memory dump for pid:"
                << process_id;
    std::move(callback).Run(false, AllocationCountMap(),
                            AllocationTracker::ContextMap());
    return;
  }

  int barrier_id = next_barrier_id_++;

  // Register for callback before requesting the dump so we don't race for the
  // signal. The callback will be issued on the allocation tracker thread so
  // need to thunk back to the I/O thread.
  Connection* connection = it->second.get();
  connection->tracker.SnapshotOnBarrier(barrier_id, std::move(task_runner),
                                        std::move(callback));
  connection->client->FlushPipe(barrier_id);
}

void MemlogConnectionManager::DoDumpProcess(
    DumpProcessArgs args,
    bool success,
    AllocationCountMap counts,
    AllocationTracker::ContextMap context) {
  if (!success) {
    std::move(args.callback).Run(false);
    return;
  }

  std::ostringstream oss;
  ExportParams params;
  params.allocs = std::move(counts);
  params.context_map = std::move(context);
  params.maps = std::move(args.maps);
  params.min_size_threshold = kMinSizeThreshold;
  params.min_count_threshold = kMinCountThreshold;
  ExportAllocationEventSetToJSON(args.pid, params, std::move(args.metadata),
                                 oss);
  std::string reply = oss.str();

  // Pass ownership of the underlying fd/HANDLE to zlib.
  base::PlatformFile platform_file = args.file.TakePlatformFile();
#if defined(OS_WIN)
  // The underlying handle |platform_file| is also closed when |fd| is closed.
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(platform_file), 0);
#else
  int fd = platform_file;
#endif
  gzFile gz_file = gzdopen(fd, "w");
  if (!gz_file) {
    DLOG(ERROR) << "Cannot compress trace file";
    std::move(args.callback).Run(false);
    return;
  }

  size_t written_bytes = gzwrite(gz_file, reply.c_str(), reply.size());
  gzclose(gz_file);

  std::move(args.callback).Run(written_bytes == reply.size());
}

void MemlogConnectionManager::DoDumpOneProcessForTracing(
    scoped_refptr<DumpProcessesForTracingTracking> tracking,
    base::ProcessId pid,
    bool success,
    AllocationCountMap counts,
    AllocationTracker::ContextMap context) {
  // All code paths through here must issue the callback when waiting_responses
  // is 0 or the browser will wait forever for the dump.
  DCHECK(tracking->waiting_responses > 0);
  tracking->waiting_responses--;

  if (!success) {
    if (tracking->waiting_responses == 0)
      std::move(tracking->callback).Run(std::move(tracking->results));
    return;
  }

  // Find the memory maps list for the given process.
  memory_instrumentation::mojom::ProcessMemoryDump* process_dump = nullptr;
  for (const auto& proc : tracking->dump->process_dumps) {
    if (proc->pid == pid) {
      process_dump = &*proc;
      break;
    }
  }
  if (!process_dump) {
    DLOG(ERROR) << "Don't have a memory dump for PID " << pid;
    if (tracking->waiting_responses == 0)
      std::move(tracking->callback).Run(std::move(tracking->results));
    return;
  }

  ExportParams params;
  params.allocs = std::move(counts);
  params.maps = std::move(process_dump->os_dump->memory_maps_for_heap_profiler);
  params.context_map = std::move(context);
  params.min_size_threshold = kMinSizeThreshold;
  params.min_count_threshold = kMinCountThreshold;

  std::ostringstream oss;
  ExportMemoryMapsAndV2StackTraceToJSON(params, oss);
  std::string reply = oss.str();

  mojo::ScopedSharedBufferHandle buffer =
      mojo::SharedBufferHandle::Create(reply.size());
  if (!buffer.is_valid()) {
    DLOG(ERROR) << "Could not create Mojo shared buffer";
  } else {
    mojo::ScopedSharedBufferMapping mapping = buffer->Map(reply.size());
    if (!mapping) {
      DLOG(ERROR) << "Could not map Mojo shared buffer";
    } else {
      memcpy(mapping.get(), reply.c_str(), reply.size());

      profiling::mojom::SharedBufferWithSizePtr result =
          profiling::mojom::SharedBufferWithSize::New();
      result->buffer = std::move(buffer);
      result->size = reply.size();
      result->pid = pid;
      tracking->results.push_back(std::move(result));
    }
  }

  // When all responses complete, issue done callback.
  if (tracking->waiting_responses == 0)
    std::move(tracking->callback).Run(std::move(tracking->results));
}

}  // namespace profiling
