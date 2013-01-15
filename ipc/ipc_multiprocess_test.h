// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MULTIPROCESS_TEST_H_
#define IPC_IPC_MULTIPROCESS_TEST_H_

#include "testing/multiprocess_func_list.h"

// Use this macro when your sub-process is using an IPCChannel to communicate
// with the test process.
// See comment below for MultiProcessTestIPCSetUp() on why this is needed.
#define MULTIPROCESS_IPC_TEST_MAIN(test_main) \
  MULTIPROCESS_TEST_MAIN_WITH_SETUP(test_main, MultiProcessTestIPCSetUp)

// Setup function used by MULTIPROCESS_IPC_TEST_MAIN.
// Registers the IPC channel as a global descriptor in the child process. This
// is needed on POSIX as the IPCChannel when created looks for a specific global
// descriptor to establish the connection to the parent process.
void MultiProcessTestIPCSetUp();

#endif  // IPC_IPC_MULTIPROCESS_TEST_H_
