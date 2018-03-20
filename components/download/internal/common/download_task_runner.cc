// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_task_runner.h"

#include "base/lazy_instance.h"
#include "base/task_scheduler/lazy_task_runner.h"
#include "build/build_config.h"

namespace download {

namespace {

#if defined(OS_WIN)
// On Windows, the download code dips into COM and the shell here and there,
// necessitating the use of a COM single-threaded apartment sequence.
base::LazyCOMSTATaskRunner g_download_task_runner =
    LAZY_COM_STA_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_VISIBLE),
        base::SingleThreadTaskRunnerThreadMode::SHARED);
#else
base::LazySequencedTaskRunner g_download_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_VISIBLE));
#endif

base::LazyInstance<scoped_refptr<base::SingleThreadTaskRunner>>::Leaky
    g_io_task_runner = LAZY_INSTANCE_INITIALIZER;

}  // namespace

scoped_refptr<base::SequencedTaskRunner> GetDownloadTaskRunner() {
  return g_download_task_runner.Get();
}

void SetIOTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  g_io_task_runner.Get() = task_runner;
}

scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() {
  return g_io_task_runner.Get();
}

}  // namespace download
