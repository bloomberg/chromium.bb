// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREAD_RESTRICTIONS_H_
#define BASE_THREAD_RESTRICTIONS_H_

namespace base {

// ThreadRestrictions helps protect threads that should not block from
// making blocking calls.  It works like this:
//
// 1) If a thread should not be allowed to make IO calls, mark it:
//      base::ThreadRestrictions::SetIOAllowed(false);
//    By default, threads *are* allowed to make IO calls.
//    In Chrome browser code, IO calls should be proxied to the File thread.
//
// 2) If a function makes a call that will go out to disk, check whether the
//    current thread is allowed:
//      base::ThreadRestrictions::AssertIOAllowed();
//
// ThreadRestrictions does nothing in release builds; it is debug-only.
//
class ThreadRestrictions {
 public:
  // Set whether the current thread to make IO calls.
  // Threads start out in the *allowed* state.
  static void SetIOAllowed(bool allowed);

  // Check whether the current thread is allowed to make IO calls,
  // and DCHECK if not.
  static void AssertIOAllowed();

 private:
  ThreadRestrictions();  // class for namespacing only
};

// In Release builds, inline the empty definitions of these functions so
// that they can be compiled out.
#ifdef NDEBUG
void ThreadRestrictions::SetIOAllowed(bool allowed) {}
void ThreadRestrictions::AssertIOAllowed() {}
#endif

}  // namespace base

#endif  // BASE_THREAD_RESTRICTIONS_H_
