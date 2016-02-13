// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "mojo/public/c/system/main.h"
#include "mojo/services/package_manager/package_manager.h"
#include "mojo/shell/public/cpp/application_runner.h"

const size_t kMaxBlockingPoolThreads = 3;

MojoResult MojoMain(MojoHandle shell_handle) {
  scoped_refptr<base::SequencedWorkerPool> blocking_pool(
      new base::SequencedWorkerPool(kMaxBlockingPoolThreads, "blocking_pool"));

  package_manager::PackageManager* package_manager =
      new package_manager::PackageManager(blocking_pool.get());
  MojoResult rv = mojo::ApplicationRunner(package_manager).Run(shell_handle);
  blocking_pool->Shutdown();
  return rv;
}
