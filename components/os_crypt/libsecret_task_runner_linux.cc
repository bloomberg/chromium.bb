// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/libsecret_task_runner_linux.h"

#include "base/task_scheduler/lazy_task_runner.h"

namespace os_crypt {

namespace {

// Use TaskPriority::USER_BLOCKING, because profile initialisation may block on
// initialising OSCrypt, which in turn may contact libsecret.
base::LazySingleThreadTaskRunner g_libsecret_thread_task_runner =
    LAZY_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_BLOCKING),
        base::SingleThreadTaskRunnerThreadMode::SHARED);

}  // namespace

// TODO(crbug.com/571003) Remove this if OSCrypt is the only client of keyring.
scoped_refptr<base::SequencedTaskRunner> GetLibsecretTaskRunner() {
  return g_libsecret_thread_task_runner.Get();
}

}  // namespace os_crypt
