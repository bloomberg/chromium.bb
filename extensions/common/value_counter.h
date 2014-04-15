// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_VALUE_COUNTER_H_
#define EXTENSIONS_COMMON_VALUE_COUNTER_H_

#include <vector>

#include "base/memory/linked_ptr.h"

namespace base {
class Value;
}

namespace extensions {

// Keeps a running count of Values, like map<Value, int>. Adding / removing
// values increments / decrements the count associated with a given Value.
//
// Add() and Remove() are linear in the number of Values in the ValueCounter,
// because there is no operator<() defined on Value, so we must iterate to find
// whether a Value is equal to an existing one.
class ValueCounter {
 public:
  ValueCounter();
  ~ValueCounter();

  // Adds |value| to the set and returns how many equal values are in the set
  // after. Does not take ownership of |value|. In the case where a Value equal
  // to |value| doesn't already exist in this map, this function makes a
  // DeepCopy() of |value|.
  int Add(const base::Value& value);

  // Removes |value| from the set and returns how many equal values are in
  // the set after.
  int Remove(const base::Value& value);

  // Same as Add() but only performs the add if the value isn't present.
  int AddIfMissing(const base::Value& value);

 private:
  class Entry {
   public:
    explicit Entry(const base::Value& value);
    ~Entry();

    int Increment();
    int Decrement();

    const base::Value* value() const { return value_.get(); }
    int count() const { return count_; }

   private:
    linked_ptr<base::Value> value_;
    int count_;

    DISALLOW_COPY_AND_ASSIGN(Entry);
  };
  typedef std::vector<linked_ptr<Entry> > EntryList;

  int AddImpl(const base::Value& value, bool increment);

  EntryList entries_;

  DISALLOW_COPY_AND_ASSIGN(ValueCounter);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_VALUE_COUNTER_H_
