// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace content {
class TestBrowserThread;
}

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

// This sets up FILE, IO and UI browser threads for testing.
// (UI thread is set to the current thread.)
class MultiThreadTestHelper {
 public:
  MultiThreadTestHelper();
  ~MultiThreadTestHelper();

  void SetUp();
  void TearDown();

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner();
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner();
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner();

 private:
  MessageLoop message_loop_;
  scoped_ptr<base::Thread> file_thread_;
  scoped_ptr<base::Thread> io_thread_;

  scoped_ptr<content::TestBrowserThread> browser_ui_thread_;
  scoped_ptr<content::TestBrowserThread> browser_file_thread_;
  scoped_ptr<content::TestBrowserThread> browser_io_thread_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_
