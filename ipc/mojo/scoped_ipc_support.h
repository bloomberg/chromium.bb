// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MOJO_SCOPED_IPC_SUPPORT_H_
#define IPC_MOJO_SCOPED_IPC_SUPPORT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "ipc/ipc_export.h"

namespace IPC {

// Performs any necessary Mojo IPC initialization on construction, and shuts
// down Mojo IPC on destruction.
//
// This should be instantiated once per process and retained as long as Mojo IPC
// is needed. The TaskRunner passed to the constructor should outlive this
// object.
class IPC_MOJO_EXPORT ScopedIPCSupport {
 public:
  ScopedIPCSupport(scoped_refptr<base::TaskRunner> io_thread_task_runner);
  ~ScopedIPCSupport();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedIPCSupport);
};

}  // namespace IPC

#endif  // IPC_MOJO_SCOPED_IPC_SUPPORT_H_
