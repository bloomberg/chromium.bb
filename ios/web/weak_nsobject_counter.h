// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEAK_NSOBJECT_COUNTER_H_
#define IOS_WEB_WEAK_NSOBJECT_COUNTER_H_

#import <Foundation/Foundation.h>

#include "base/memory/linked_ptr.h"
#include "base/threading/non_thread_safe.h"

namespace web {

// WeakNSObjectCounter is a class to maintain a counter of all the objects
// that are added using |Insert| that are alive.
// This class is not thread safe and objects must be destroyed and created on
// the same thread.
// TODO(stuartmorgan): Remove this once ARC is supported.
class WeakNSObjectCounter : public base::NonThreadSafe {
 public:
  WeakNSObjectCounter();
  ~WeakNSObjectCounter();
  // Inserts an object. |object| cannot be nil.
  void Insert(id object);
  // Returns the count of all active objects that have been inserted.
  NSUInteger Size() const;
 private:
  // The object that maintains the number of active items.
  linked_ptr<NSUInteger> counter_;
};

}  // namespace web

#endif  // IOS_WEB_WEAK_NSOBJECT_COUNTER_H_
