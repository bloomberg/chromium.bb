// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_THREAD_TEST_HELPER_H_
#define CHROME_TEST_THREAD_TEST_HELPER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/browser_thread.h"

// Helper class that executes code on a given thread while blocking on the
// invoking thread (normally the UI thread). To use, derive from this class and
// overwrite RunTest. An alternative use of this class is to use it directly.
// It will then block until all pending tasks on a given thread have been
// executed.
class ThreadTestHelper : public base::RefCountedThreadSafe<ThreadTestHelper> {
 public:
  explicit ThreadTestHelper(BrowserThread::ID thread_id);

  // True if RunTest() was successfully executed on the target thread.
  bool Run() WARN_UNUSED_RESULT;

  virtual void RunTest();

 protected:
  friend class base::RefCountedThreadSafe<ThreadTestHelper>;

  virtual ~ThreadTestHelper();

  // Use this method to store the result of RunTest().
  void set_test_result(bool test_result) { test_result_ = test_result; }

 private:
  void RunInThread();

  bool test_result_;
  BrowserThread::ID thread_id_;
  base::WaitableEvent done_event_;

  DISALLOW_COPY_AND_ASSIGN(ThreadTestHelper);
};

#endif  // CHROME_TEST_THREAD_TEST_HELPER_H_
