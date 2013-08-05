// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

using content::BrowserThread;

namespace sync_file_system {

template <typename R>
void AssignAndQuit(base::RunLoop* run_loop, R* result_out, R result) {
  DCHECK(result_out);
  DCHECK(run_loop);
  *result_out = result;
  run_loop->Quit();
}

template <typename R> base::Callback<void(R)>
AssignAndQuitCallback(base::RunLoop* run_loop, R* result) {
  return base::Bind(&AssignAndQuit<R>, run_loop, base::Unretained(result));
}

// Instantiate versions we know callers will need.
template base::Callback<void(SyncStatusCode)>
AssignAndQuitCallback(base::RunLoop*, SyncStatusCode*);

}  // namespace sync_file_system
