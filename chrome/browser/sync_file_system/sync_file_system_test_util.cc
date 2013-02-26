// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "content/public/test/test_browser_thread.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

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
    : file_thread_(new base::Thread("File_Thread")),
      io_thread_(new base::Thread("IO_Thread")) {}

MultiThreadTestHelper::~MultiThreadTestHelper() {}

void MultiThreadTestHelper::SetUp() {
  file_thread_->Start();
  io_thread_->StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));

  browser_ui_thread_.reset(
      new TestBrowserThread(BrowserThread::UI,
                            MessageLoop::current()));
  browser_file_thread_.reset(
      new TestBrowserThread(BrowserThread::FILE,
                            file_thread_->message_loop()));
  browser_io_thread_.reset(
      new TestBrowserThread(BrowserThread::IO,
                            io_thread_->message_loop()));

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

  io_thread_->Stop();
  file_thread_->Stop();
}

}  // namespace sync_file_system
