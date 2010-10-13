// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREAD_CHECKER_H_
#define BASE_THREAD_CHECKER_H_
#pragma once

#include "base/platform_thread.h"
#include "base/scoped_ptr.h"

// Before using this class, please consider using NonThreadSafe as it
// makes it much easier to determine the nature of your class.
//
// A helper class used to help verify that some methods of a class are
// called from the same thread.  One can inherit from this class and use
// CalledOnValidThread() to verify.
//
// Inheriting from class indicates that one must be careful when using the class
// with multiple threads. However, it is up to the class document to indicate
// how it can be used with threads.
//
// Example:
// class MyClass : public ThreadChecker {
//  public:
//   void Foo() {
//     DCHECK(CalledOnValidThread());
//     ... (do stuff) ...
//   }
// }
//
// In Release mode, CalledOnValidThread will always return true.
//
#ifndef NDEBUG
class ThreadChecker {
 public:
  ThreadChecker();
  ~ThreadChecker();

  bool CalledOnValidThread() const;

  // Changes the thread that is checked for in CalledOnValidThread.  This may
  // be useful when an object may be created on one thread and then used
  // exclusively on another thread.
  void DetachFromThread();

 private:
  void EnsureThreadIdAssigned() const;

  // This is mutable so that CalledOnValidThread can set it.
  mutable scoped_ptr<PlatformThreadId> valid_thread_id_;
};
#else
// Do nothing in release mode.
class ThreadChecker {
 public:
  bool CalledOnValidThread() const {
    return true;
  }

  void DetachFromThread() {}
};
#endif  // NDEBUG

#endif  // BASE_THREAD_CHECKER_H_
