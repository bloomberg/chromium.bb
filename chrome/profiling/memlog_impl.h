// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_IMPL_H_
#define CHROME_PROFILING_MEMLOG_IMPL_H_

#include "base/files/platform_file.h"
#include "base/macros.h"
#include "chrome/common/profiling/memlog.mojom.h"
#include "chrome/profiling/backtrace_storage.h"
#include "chrome/profiling/memlog_connection_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace profiling {

// Represents the profiling process side of the profiling <-> browser
// connection.
//
// This class lives on the main thread.
//
// TODO(ajwong): Why do all these mojo things get spawned on the main thread
// instead of the IO thread?
class MemlogImpl : public mojom::Memlog {
 public:
  MemlogImpl();
  ~MemlogImpl() override;

  void AddSender(base::ProcessId pid, mojo::ScopedHandle sender_pipe) override;
  void DumpProcess(base::ProcessId pid,
                   mojo::ScopedHandle output_file) override;

 private:
  // Helper for managing lifetime of MemlogConnectionManager.
  struct DeleteOnRunner {
    DeleteOnRunner(const tracked_objects::Location& location,
                   base::SequencedTaskRunner* runner)
        : location(location), runner(runner) {}

    void operator()(MemlogConnectionManager* ptr) {
      runner->DeleteSoon(location, ptr);
    }

    const tracked_objects::Location& location;
    base::SequencedTaskRunner* runner;
  };

  scoped_refptr<base::SequencedTaskRunner> io_runner_;
  BacktraceStorage backtrace_storage_;
  std::unique_ptr<MemlogConnectionManager, DeleteOnRunner> connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(MemlogImpl);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_IMPL_H_
