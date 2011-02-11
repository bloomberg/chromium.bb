// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/thread_test_helper.h"

ThreadTestHelper::ThreadTestHelper(BrowserThread::ID thread_id)
    : test_result_(false),
      thread_id_(thread_id),
      done_event_(false, false) {
}

bool ThreadTestHelper::Run() {
  if (!BrowserThread::PostTask(thread_id_, FROM_HERE, NewRunnableMethod(
          this, &ThreadTestHelper::RunInThread))) {
    return false;
  }
  done_event_.Wait();
  return test_result_;
}

void ThreadTestHelper::RunTest() { set_test_result(true); }

ThreadTestHelper::~ThreadTestHelper() {}

void ThreadTestHelper::RunInThread() {
  RunTest();
  done_event_.Signal();
}
