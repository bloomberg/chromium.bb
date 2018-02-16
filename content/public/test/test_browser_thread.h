// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_H_
#define CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class MessageLoop;
}

namespace content {

class TestBrowserThreadImpl;

// DEPRECATED: use TestBrowserThreadBundle instead. See http://crbug.com/272091
// A BrowserThread for unit tests; this lets unit tests in chrome/ create
// BrowserThread instances.
class TestBrowserThread {
 public:
  explicit TestBrowserThread(BrowserThread::ID identifier);
  TestBrowserThread(BrowserThread::ID identifier,
                    base::MessageLoop* message_loop);
  ~TestBrowserThread();

  // We provide a subset of the capabilities of the Thread interface
  // to enable certain unit tests.  To avoid a stronger dependency of
  // the internals of BrowserThread, we do not provide the full Thread
  // interface.

  // Starts the thread with a generic message loop.
  bool Start();

  // Starts the thread with a generic message loop and waits for the
  // thread to run.
  bool StartAndWaitForTesting();

  // Starts the thread with an IOThread message loop.
  bool StartIOThread();

  // Initializes the BrowserThreadDelegate.
  void InitIOThreadDelegate();

  // Stops the thread.
  void Stop();

  // Returns true if the thread is running.
  bool IsRunning();

 private:
  std::unique_ptr<TestBrowserThreadImpl> impl_;

  const BrowserThread::ID identifier_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserThread);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_H_
