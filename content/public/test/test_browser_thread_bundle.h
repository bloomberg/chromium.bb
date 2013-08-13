// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TestBrowserThreadBundle is a convenience class for creating a set of
// TestBrowserThreads in unit tests.  For most tests, it is sufficient to
// just instantiate the TestBrowserThreadBundle as a member variable.
//
// By default, all of the created TestBrowserThreads will be backed by a single
// shared MessageLoop. If a test truly needs separate threads, it can do
// so by passing the appropriate combination of RealThreadsMask values during
// the TestBrowserThreadBundle construction.
//
// The TestBrowserThreadBundle will attempt to drain the MessageLoop on
// destruction. Sometimes a test needs to drain currently enqueued tasks
// mid-test. Browser tests should call content::RunAllPendingInMessageLoop().
// Unit tests should use base::RunLoop (e.g., base::RunLoop().RunUntilIdle()).
// TODO(phajdan.jr): Revise this comment after switch to Aura.
//
// The TestBrowserThreadBundle will also flush the blocking pool on destruction.
// We do this to avoid memory leaks, particularly in the case of threads posting
// tasks to the blocking pool via PostTaskAndReply. By ensuring that the tasks
// are run while the originating TestBroswserThreads still exist, we prevent
// leakage of PostTaskAndReplyRelay objects. We also flush the blocking pool
// again at the point where it would normally be shut down, to better simulate
// the normal thread shutdown process.
//
// Some tests using the IO thread expect a MessageLoopForIO. Passing
// IO_MAINLOOP will use a MessageLoopForIO for the main MessageLoop.
// Most of the time, this avoids needing to use a REAL_IO_THREAD.

#ifndef CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_TEST_BUNDLE_H_
#define CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_TEST_BUNDLE_H_

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class MessageLoop;
}  // namespace base

namespace content {

class TestBrowserThread;

class TestBrowserThreadBundle {
 public:
  // Used to specify the type of MessageLoop that backs the UI thread, and
  // which of the named BrowserThreads should be backed by a real
  // threads. The UI thread is always the main thread in a unit test.
  enum Options {
    DEFAULT = 0x00,
    IO_MAINLOOP = 0x01,
    REAL_DB_THREAD = 0x02,
    REAL_FILE_THREAD = 0x08,
    REAL_FILE_USER_BLOCKING_THREAD = 0x10,
    REAL_PROCESS_LAUNCHER_THREAD = 0x20,
    REAL_CACHE_THREAD = 0x40,
    REAL_IO_THREAD = 0x80,
  };

  TestBrowserThreadBundle();
  explicit TestBrowserThreadBundle(int options);

  ~TestBrowserThreadBundle();

 private:
  void Init(int options);

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<TestBrowserThread> ui_thread_;
  scoped_ptr<TestBrowserThread> db_thread_;
  scoped_ptr<TestBrowserThread> file_thread_;
  scoped_ptr<TestBrowserThread> file_user_blocking_thread_;
  scoped_ptr<TestBrowserThread> process_launcher_thread_;
  scoped_ptr<TestBrowserThread> cache_thread_;
  scoped_ptr<TestBrowserThread> io_thread_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserThreadBundle);
};

}  // namespace content

#endif /* CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_TEST_BUNDLE_H_ */
