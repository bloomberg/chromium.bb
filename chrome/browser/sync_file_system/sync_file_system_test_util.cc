// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "webkit/browser/fileapi/syncable/sync_status_code.h"

using content::BrowserThread;
using content::TestBrowserThread;

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

MultiThreadTestHelper::MultiThreadTestHelper()
    : thread_bundle_(new content::TestBrowserThreadBundle(
          content::TestBrowserThreadBundle::REAL_FILE_THREAD |
          content::TestBrowserThreadBundle::REAL_IO_THREAD)) {
}

MultiThreadTestHelper::~MultiThreadTestHelper() {}

void MultiThreadTestHelper::SetUp() {
  ui_task_runner_ =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  file_task_runner_ =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
  io_task_runner_ =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

void MultiThreadTestHelper::TearDown() {
  // Make sure we give some more time to finish tasks on the FILE thread
  // before stopping IO/FILE threads.
  base::RunLoop run_loop;
  file_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&base::DoNothing), run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace sync_file_system
