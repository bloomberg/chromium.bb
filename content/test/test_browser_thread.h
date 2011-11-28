// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BROWSER_THREAD_H_
#define CONTENT_TEST_TEST_BROWSER_THREAD_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"

class MessageLoop;

namespace base {
class Thread;
}

namespace content {

class TestBrowserThreadImpl;

// A BrowserThread for unit tests; this lets unit tests in chrome/
// create BrowserThread instances.
class TestBrowserThread {
 public:
  explicit TestBrowserThread(BrowserThread::ID identifier);
  TestBrowserThread(BrowserThread::ID identifier, MessageLoop* message_loop);
  ~TestBrowserThread();

  // We provide a subset of the capabilities of the Thread interface
  // to enable certain unit tests.  To avoid a stronger dependency of
  // the internals of BrowserThread, we do not provide the full Thread
  // interface.

  // Starts the thread with a generic message loop.
  bool Start();

  // Starts the thread with an IOThread message loop.
  bool StartIOThread();

  // Stops the thread.
  void Stop();

  // Returns true if the thread is running.
  bool IsRunning();

  // Returns a Thread pointer for the thread. This should not be used
  // in new tests.
  base::Thread* DeprecatedGetThreadObject();

  // Sets the message loop to use for the thread. This should not be
  // used in new tests.
  void DeprecatedSetMessageLoop(MessageLoop* loop);

 private:
  scoped_ptr<TestBrowserThreadImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserThread);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BROWSER_THREAD_H_
