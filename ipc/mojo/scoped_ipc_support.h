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

// Perform any necessary Mojo IPC initialization. A ScopedIPCSupport object
// should be instantiated and retained by any component which makes direct calls
// to the Mojo EDK. This is used to ensure that the EDK is initialized within
// the current process and that it is shutdown cleanly when no longer in use.
//
// NOTE: Unless you are making explicit calls to functions in the
// mojo::embedder namespace, you almost definitely DO NOT need this and should
// not be using it.
class IPC_MOJO_EXPORT ScopedIPCSupport {
 public:
  ScopedIPCSupport(scoped_refptr<base::TaskRunner> io_thread_task_runner);
  ~ScopedIPCSupport();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedIPCSupport);
};

}  // namespace IPC

#endif  // IPC_MOJO_SCOPED_IPC_SUPPORT_H_
