// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LEAK_TRACKER_H_
#define BASE_LEAK_TRACKER_H_

#ifndef NDEBUG
#include "base/debug_util.h"
#include "base/linked_list.h"
#include "base/logging.h"
#endif

// LeakTracker is a debug helper to verify that all instances of a class
// have been destroyed.
//
// It is particularly useful for classes that are bound to a single thread --
// before destroying that thread, one can check that there are no remaining
// instances of that class.
//
// For example, to enable leak tracking for class URLRequest, start by
// adding a member variable of type LeakTracker<URLRequest>.
//
//   class URLRequest {
//     ...
//    private:
//     base::LeakTracker<URLRequest> leak_tracker_;
//   };
//
//
// Next, when we believe all instances of URLRequest have been deleted:
//
//   LeakTracker<URLRequest>::CheckForLeaks();
//
// Should the check fail (because there are live instances of URLRequest),
// then the allocation callstack for each leaked instances is dumped to
// the error log.
//
// In RELEASE mode the check has no effect.

namespace base {

#ifdef NDEBUG

// In release mode we do nothing.
template<typename T>
class LeakTracker {
 public:
  static void CheckForLeaks() {}
  static int NumLiveInstances() { return -1; }
};

#else

// In debug mode we track where the object was allocated from.

template<typename T>
class LeakTracker : public LinkNode<LeakTracker<T> > {
 public:
  LeakTracker() {
    instances()->Append(this);
  }

  ~LeakTracker() {
    this->RemoveFromList();
  }

  static void CheckForLeaks() {
    // Walk the allocation list and print each entry it contains.
    int count = 0;
    for (LinkNode<LeakTracker<T> >* node = instances()->head();
         node != instances()->end();
         node = node->next()) {
      ++count;
      LOG(ERROR) << "Leaked " << node << " which was allocated by:";
      node->value()->allocation_stack_.PrintBacktrace();
    }
    DCHECK_EQ(0, count);
  }

  static int NumLiveInstances() {
    // Walk the allocation list and count how many entries it has.
    int count = 0;
    for (LinkNode<LeakTracker<T> >* node = instances()->head();
         node != instances()->end();
         node = node->next()) {
      ++count;
    }
    return count;
  }

 private:
  // Each specialization of LeakTracker gets its own static storage.
  static LinkedList<LeakTracker<T> >* instances() {
    static LinkedList<LeakTracker<T> > list;
    return &list;
  }

  StackTrace allocation_stack_;
};

#endif  // NDEBUG

}  // namespace base

#endif  // BASE_LEAK_TRACKER_H_
