// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_connection_manager.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "chrome/profiling/allocation_tracker.h"
#include "chrome/profiling/json_exporter.h"
#include "chrome/profiling/memlog_receiver_pipe.h"
#include "chrome/profiling/memlog_stream_parser.h"
#include "third_party/zlib/zlib.h"

#if defined(OS_WIN)
#include <io.h>
#endif

namespace profiling {

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

MemlogConnectionManager::MemlogConnectionManager(
    scoped_refptr<base::SequencedTaskRunner> io_runner)
    : io_runner_(std::move(io_runner)) {}

MemlogConnectionManager::~MemlogConnectionManager() {}

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

  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MemlogReceiverPipe::StartReadingOnIOThread, new_pipe));
}

void MemlogConnectionManager::OnConnectionComplete(base::ProcessId pid) {
  base::AutoLock lock(connections_lock_);
  auto found = connections_.find(pid);
  CHECK(found != connections_.end());
  found->second.release();
  connections_.erase(found);
}

// Posts back to the given thread the connection complete message.
void MemlogConnectionManager::OnConnectionCompleteThunk(
    scoped_refptr<base::SingleThreadTaskRunner> main_loop,
    base::ProcessId pid) {
  // This code is called by the allocation tracker which is owned by the
  // connection manager. When we tell the connection manager a connection is
  // done, we know the connection manager will still be in scope.
  main_loop->PostTask(FROM_HERE,
                      base::Bind(&MemlogConnectionManager::OnConnectionComplete,
                                 base::Unretained(this), pid));
}

void MemlogConnectionManager::DumpProcess(
    base::ProcessId pid,
    std::unique_ptr<base::DictionaryValue> metadata,
    const std::vector<memory_instrumentation::mojom::VmRegionPtr>& maps,
    base::File output_file) {
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

  auto it = connections_.find(pid);
  if (it == connections_.end()) {
    LOG(ERROR) << "No connections found for memory dump for pid:" << pid;
    return;
  }

  Connection* connection = it->second.get();

  std::ostringstream oss;
  ExportAllocationEventSetToJSON(pid, connection->tracker.live_allocs(), maps,
                                 oss, std::move(metadata));
  std::string reply = oss.str();

  // Pass ownership of the underlying fd/HANDLE to zlib.
  base::PlatformFile platform_file = output_file.TakePlatformFile();
#if defined(OS_WIN)
  // The underlying handle |platform_file| is also closed when |fd| is closed.
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(platform_file), 0);
#else
  int fd = platform_file;
#endif
  gzFile gz_file = gzdopen(fd, "w");
  gzwrite(gz_file, reply.c_str(), reply.size());
  gzclose(gz_file);
}

}  // namespace profiling
