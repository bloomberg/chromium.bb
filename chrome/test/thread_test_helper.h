// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_THREAD_TEST_HELPER_H_
#define CHROME_TEST_THREAD_TEST_HELPER_H_
#pragma once

#include "base/ref_counted.h"
#include "base/waitable_event.h"
#include "chrome/browser/chrome_thread.h"

// Helper class that executes code on a given thread while blocking on the
// invoking thread (normally the UI thread). To use, derive from this class and
// overwrite RunTest. An alternative use of this class is to use it directly.
// It will then block until all pending tasks on a given thread have been
// executed.
class ThreadTestHelper : public base::RefCountedThreadSafe<ThreadTestHelper> {
 public:
  explicit ThreadTestHelper(ChromeThread::ID thread_id)
    : test_result_(false),
      thread_id_(thread_id),
      done_event_(false, false) {
  }

  // True if RunTest() was successfully executed on the target thread.
  bool Run() WARN_UNUSED_RESULT {
    if (!ChromeThread::PostTask(thread_id_, FROM_HERE, NewRunnableMethod(
        this, &ThreadTestHelper::RunInThread))) {
      return false;
    }
    done_event_.Wait();
    return test_result_;
  }

  virtual void RunTest() { set_test_result(true); }

 protected:
  friend class base::RefCountedThreadSafe<ThreadTestHelper>;

  virtual ~ThreadTestHelper() {}

  // Use this method to store the result of RunTest().
  void set_test_result(bool test_result) { test_result_ = test_result; }

 private:
  void RunInThread() {
    RunTest();
    done_event_.Signal();
  }

  bool test_result_;
  ChromeThread::ID thread_id_;
  base::WaitableEvent done_event_;

  DISALLOW_COPY_AND_ASSIGN(ThreadTestHelper);
};

#endif  // CHROME_TEST_THREAD_TEST_HELPER_H_
