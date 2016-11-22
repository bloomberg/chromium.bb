// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_TEST_SCOPED_IPC_SUPPORT_H_
#define MOJO_EDK_TEST_SCOPED_IPC_SUPPORT_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/process_delegate.h"

namespace mojo {
namespace edk {
namespace test {

base::TaskRunner* GetIoTaskRunner();

// A simple class that calls |InitIPCSupport()| on construction and
// |ShutdownIPCSupport()| on destruction.
class ScopedIPCSupport : public ProcessDelegate {
 public:
  explicit ScopedIPCSupport(
      scoped_refptr<base::TaskRunner> io_thread_task_runner);
  ~ScopedIPCSupport() override;

 private:
  // |ProcessDelegate| implementation:
  // Note: Executed on the I/O thread.
  void OnShutdownComplete() override;

  base::Closure shutdown_closure_;
  base::WaitableEvent shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(ScopedIPCSupport);
};

}  // namespace test
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_TEST_SCOPED_IPC_SUPPORT_H_
