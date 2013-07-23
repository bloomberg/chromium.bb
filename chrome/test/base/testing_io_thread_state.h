// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_IO_THREAD_STATE_H_
#define CHROME_TEST_BASE_TESTING_IO_THREAD_STATE_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

class IOThread;

namespace chrome {

// Convenience class for creating an IOThread object in unittests.
// Usual usage is to create one of these in the test fixture, after the
// BrowserThreadBundle and TestingBrowserProcess have been initialized.
//
// If code requires the use of io thread globals, those can be set by
// accessing io_thread_state()->globals() on the IO thread during test setup.
class TestingIOThreadState {
 public:
  TestingIOThreadState();
  ~TestingIOThreadState();
  IOThread* io_thread_state() { return io_thread_state_.get(); }

 private:
  void Initialize(const base::Closure& done);
  void Shutdown(const base::Closure& done);

  scoped_ptr<IOThread> io_thread_state_;

  DISALLOW_COPY_AND_ASSIGN(TestingIOThreadState);
};

}  // namespace chrome

#endif  // CHROME_TEST_BASE_TESTING_IO_THREAD_STATE_H_
