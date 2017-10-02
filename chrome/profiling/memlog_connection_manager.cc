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

struct MemlogConnectionManager::Connection {
  Connection(AllocationTracker::CompleteCallback complete_cb,
             BacktraceStorage* backtrace_storage,
             base::ProcessId pid,
             scoped_refptr<MemlogReceiverPipe> p)
      : thread(base::StringPrintf("Sender %lld thread",
                                  static_cast<long long>(pid))),
        pipe(p),
        tracker(std::move(complete_cb), backtrace_storage) {}

  ~Connection() {
    // The parser may outlive this class because it's refcounted, make sure no
    // callbacks are issued.
    parser->DisconnectReceivers();
  }

  base::Thread thread;

  scoped_refptr<MemlogReceiverPipe> pipe;
  scoped_refptr<MemlogStreamParser> parser;
  AllocationTracker tracker;
};

MemlogConnectionManager::MemlogConnectionManager() : weak_factory_(this) {}
MemlogConnectionManager::~MemlogConnectionManager() = default;

void MemlogConnectionManager::OnNewConnection(base::ScopedPlatformFile file,
                                              base::ProcessId pid) {
  base::AutoLock lock(connections_lock_);
  DCHECK(connections_.find(pid) == connections_.end());

  scoped_refptr<MemlogReceiverPipe> new_pipe =
      new MemlogReceiverPipe(std::move(file));
  // Task to post to clean up the connection. Don't need to retain |this| since
  // it will be called by objects owned by the MemlogConnectionManager.
  AllocationTracker::CompleteCallback complete_cb = base::BindOnce(
      &MemlogConnectionManager::OnConnectionCompleteThunk,
      base::Unretained(this), base::MessageLoop::current()->task_runner(), pid);

  std::unique_ptr<Connection> connection = base::MakeUnique<Connection>(
      std::move(complete_cb), &backtrace_storage_, pid, new_pipe);
  connection->thread.Start();

  connection->parser = new MemlogStreamParser(&connection->tracker);
  new_pipe->SetReceiver(connection->thread.task_runner(), connection->parser);

  connections_[pid] = std::move(connection);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&MemlogReceiverPipe::StartReadingOnIOThread, new_pipe));
}

void MemlogConnectionManager::OnConnectionComplete(base::ProcessId pid) {
  base::AutoLock lock(connections_lock_);
  auto found = connections_.find(pid);
  CHECK(found != connections_.end());
  connections_.erase(found);
}

// Posts back to the given thread the connection complete message.
void MemlogConnectionManager::OnConnectionCompleteThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::ProcessId pid) {
  task_runner->PostTask(
      FROM_HERE, base::Bind(&MemlogConnectionManager::OnConnectionComplete,
                            weak_factory_.GetWeakPtr(), pid));
}

void MemlogConnectionManager::DumpProcess(DumpProcessArgs args,
                                          bool hop_to_connection_thread) {
  base::AutoLock lock(connections_lock_);

  // Lock all connections to prevent deallocations of atoms from
  // BacktraceStorage. This only works if no new connections are made, which
  // connections_lock_ guarantees.
  std::vector<std::unique_ptr<base::AutoLock>> locks;
  for (auto& it : connections_) {
    Connection* connection = it.second.get();
    locks.push_back(
        base::MakeUnique<base::AutoLock>(*connection->parser->GetLock()));
  }

  auto it = connections_.find(args.pid);
  if (it == connections_.end()) {
    DLOG(ERROR) << "No connections found for memory dump for pid:" << args.pid;
    std::move(args.callback).Run(false);
    return;
  }

  Connection* connection = it->second.get();

  if (hop_to_connection_thread) {
    connection->thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&MemlogConnectionManager::HopToConnectionThread,
                       weak_factory_.GetWeakPtr(), std::move(args),
                       base::ThreadTaskRunnerHandle::Get()));
    return;
  }

  std::ostringstream oss;
  ExportParams params;
  params.allocs = connection->tracker.GetCounts();
  params.context_map = &connection->tracker.context();
  params.maps = &args.maps;
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

void MemlogConnectionManager::HopToConnectionThread(
    base::WeakPtr<MemlogConnectionManager> manager,
    DumpProcessArgs args,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MemlogConnectionManager::DumpProcess,
                                std::move(manager), std::move(args), false));
}

void MemlogConnectionManager::DumpProcessesForTracing(
    mojom::Memlog::DumpProcessesForTracingCallback callback,
    memory_instrumentation::mojom::GlobalMemoryDumpPtr dump) {
  base::AutoLock lock(connections_lock_);

  // Lock all connections to prevent deallocations of atoms from
  // BacktraceStorage. This only works if no new connections are made, which
  // connections_lock_ guarantees.
  std::vector<std::unique_ptr<base::AutoLock>> locks;
  for (auto& it : connections_) {
    Connection* connection = it.second.get();
    locks.push_back(
        base::MakeUnique<base::AutoLock>(*connection->parser->GetLock()));
  }

  std::vector<profiling::mojom::SharedBufferWithSizePtr> results;
  results.reserve(connections_.size());
  for (auto& it : connections_) {
    base::ProcessId pid = it.first;
    Connection* connection = it.second.get();
    memory_instrumentation::mojom::ProcessMemoryDump* process_dump = nullptr;
    for (const auto& proc : dump->process_dumps) {
      if (proc->pid == pid) {
        process_dump = &*proc;
        break;
      }
    }
    if (!process_dump) {
      DLOG(ERROR) << "Don't have a memory dump for PID " << pid;
      continue;
    }
    const std::vector<memory_instrumentation::mojom::VmRegionPtr>& maps =
        process_dump->os_dump->memory_maps_for_heap_profiler;

    std::ostringstream oss;
    ExportParams params;
    params.allocs = connection->tracker.GetCounts();
    params.maps = &maps;
    params.context_map = &connection->tracker.context();
    params.min_size_threshold = kMinSizeThreshold;
    params.min_count_threshold = kMinCountThreshold;
    ExportMemoryMapsAndV2StackTraceToJSON(params, oss);
    std::string reply = oss.str();

    mojo::ScopedSharedBufferHandle buffer =
        mojo::SharedBufferHandle::Create(reply.size());
    if (!buffer.is_valid()) {
      DLOG(ERROR) << "Could not create Mojo shared buffer";
      continue;
    }

    mojo::ScopedSharedBufferMapping mapping = buffer->Map(reply.size());
    if (!mapping) {
      DLOG(ERROR) << "Could not map Mojo shared buffer";
      continue;
    }

    memcpy(mapping.get(), reply.c_str(), reply.size());

    profiling::mojom::SharedBufferWithSizePtr result =
        profiling::mojom::SharedBufferWithSize::New();
    result->buffer = std::move(buffer);
    result->size = reply.size();
    result->pid = pid;
    results.push_back(std::move(result));
  }
  std::move(callback).Run(std::move(results));
}

MemlogConnectionManager::DumpProcessArgs::DumpProcessArgs() = default;
MemlogConnectionManager::DumpProcessArgs::~DumpProcessArgs() = default;
MemlogConnectionManager::DumpProcessArgs::DumpProcessArgs(DumpProcessArgs&&) =
    default;

}  // namespace profiling
