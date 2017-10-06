// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_IMPL_H_
#define CHROME_PROFILING_MEMLOG_IMPL_H_

#include "base/files/file.h"
#include "base/files/platform_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/profiling/memlog.mojom.h"
#include "chrome/profiling/memlog_connection_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
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

  void AddSender(base::ProcessId pid,
                 mojo::ScopedHandle sender_pipe,
                 AddSenderCallback callback) override;
  void DumpProcess(base::ProcessId pid,
                   mojo::ScopedHandle output_file,
                   std::unique_ptr<base::DictionaryValue> metadata,
                   DumpProcessCallback callback) override;
  void DumpProcessesForTracing(
      DumpProcessesForTracingCallback callback) override;

 private:
  void OnGetVmRegionsCompleteForDumpProcess(
      base::ProcessId pid,
      std::unique_ptr<base::DictionaryValue> metadata,
      base::File file,
      DumpProcessCallback callback,
      bool success,
      memory_instrumentation::mojom::GlobalMemoryDumpPtr dump);
  void OnGetVmRegionsCompleteForDumpProcessesForTracing(
      DumpProcessesForTracingCallback callback,
      bool success,
      memory_instrumentation::mojom::GlobalMemoryDumpPtr dump);

  std::unique_ptr<MemlogConnectionManager> connection_manager_;

  // Must be last.
  base::WeakPtrFactory<MemlogImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MemlogImpl);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_IMPL_H_
