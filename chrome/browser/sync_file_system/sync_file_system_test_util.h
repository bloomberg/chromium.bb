// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"

namespace base {
class RunLoop;
class SingleThreadTaskRunner;
class Thread;
}

namespace content {
class TestBrowserThread;
}

namespace sync_file_system {

template <typename R>
void AssignAndQuit(base::RunLoop* run_loop, R* result_out, R result);

template <typename R> base::Callback<void(R)>
AssignAndQuitCallback(base::RunLoop* run_loop, R* result);

// This sets up FILE, IO and UI browser threads for testing.
// (UI thread is set to the current thread.)
class MultiThreadTestHelper {
 public:
  MultiThreadTestHelper();
  ~MultiThreadTestHelper();

  void SetUp();
  void TearDown();

  MessageLoop* message_loop() { return &message_loop_; }

  base::SingleThreadTaskRunner* ui_task_runner() {
    return ui_task_runner_.get();
  }

  base::SingleThreadTaskRunner* file_task_runner() {
    return file_task_runner_.get();
  }

  base::SingleThreadTaskRunner* io_task_runner() {
    return io_task_runner_.get();
  }

 private:
  MessageLoop message_loop_;
  scoped_ptr<base::Thread> file_thread_;
  scoped_ptr<base::Thread> io_thread_;

  scoped_ptr<content::TestBrowserThread> browser_ui_thread_;
  scoped_ptr<content::TestBrowserThread> browser_file_thread_;
  scoped_ptr<content::TestBrowserThread> browser_io_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_
