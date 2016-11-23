// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_TEST_WEB_THREAD_BUNDLE_H_
#define IOS_WEB_PUBLIC_TEST_TEST_WEB_THREAD_BUNDLE_H_

// TestWebThreadBundle is a convenience class for creating a set of
// TestWebThreads in unit tests.  For most tests, it is sufficient to
// just instantiate the TestWebThreadBundle as a member variable.
//
// By default, all of the created TestWebThreads will be backed by a single
// shared MessageLoop. If a test truly needs separate threads, it can do
// so by passing the appropriate combination of RealThreadsMask values during
// the TestWebThreadBundle construction.
//
// The TestWebThreadBundle will attempt to drain the MessageLoop on
// destruction. Sometimes a test needs to drain currently enqueued tasks
// mid-test.
//
// The TestWebThreadBundle will also flush the blocking pool on destruction.
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

#include <memory>

#include "base/macros.h"

namespace base {
class MessageLoop;
}

namespace web {

class TestWebThread;

class TestWebThreadBundle {
 public:
  // Used to specify the type of MessageLoop that backs the UI thread, and
  // which of the named WebThreads should be backed by a real
  // threads. The UI thread is always the main thread in a unit test.
  enum Options {
    DEFAULT = 0x00,
    IO_MAINLOOP = 0x01,
    REAL_DB_THREAD = 0x02,
    REAL_FILE_THREAD = 0x04,
    REAL_IO_THREAD = 0x08,
  };

  TestWebThreadBundle();
  explicit TestWebThreadBundle(int options);

  ~TestWebThreadBundle();

 private:
  void Init(int options);

  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<TestWebThread> ui_thread_;
  std::unique_ptr<TestWebThread> db_thread_;
  std::unique_ptr<TestWebThread> file_thread_;
  std::unique_ptr<TestWebThread> file_user_blocking_thread_;
  std::unique_ptr<TestWebThread> cache_thread_;
  std::unique_ptr<TestWebThread> io_thread_;

  DISALLOW_COPY_AND_ASSIGN(TestWebThreadBundle);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_TEST_WEB_THREAD_BUNDLE_H_
