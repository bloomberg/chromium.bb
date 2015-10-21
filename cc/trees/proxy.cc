// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy.h"

#include "base/single_thread_task_runner.h"
#include "cc/trees/blocking_task_runner.h"

namespace cc {

Proxy::Proxy(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
             scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner)
    : TaskRunnerProvider(main_task_runner, impl_task_runner) {}

Proxy::~Proxy() {
  DCHECK(IsMainThread());
}

}  // namespace cc
