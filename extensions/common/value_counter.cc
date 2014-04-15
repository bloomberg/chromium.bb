// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/value_counter.h"

#include <algorithm>

#include "base/values.h"

namespace extensions {

ValueCounter::ValueCounter() {}

ValueCounter::~ValueCounter() {}

ValueCounter::Entry::Entry(const base::Value& value)
    : value_(value.DeepCopy()), count_(1) {}

ValueCounter::Entry::~Entry() {}

int ValueCounter::Entry::Increment() { return ++count_; }

int ValueCounter::Entry::Decrement() { return --count_; }

int ValueCounter::Add(const base::Value& value) { return AddImpl(value, true); }

int ValueCounter::Remove(const base::Value& value) {
  for (EntryList::iterator it = entries_.begin(); it != entries_.end(); it++) {
    (*it)->value()->GetType();
    if ((*it)->value()->Equals(&value)) {
      int remaining = (*it)->Decrement();
      if (remaining == 0) {
        std::swap(*it, entries_.back());
        entries_.pop_back();
      }
      return remaining;
    }
  }
  return 0;
}

int ValueCounter::AddIfMissing(const base::Value& value) {
  return AddImpl(value, false);
}

int ValueCounter::AddImpl(const base::Value& value, bool increment) {
  for (EntryList::iterator it = entries_.begin(); it != entries_.end(); it++) {
    if ((*it)->value()->Equals(&value))
      return increment ? (*it)->Increment() : (*it)->count();
  }
  entries_.push_back(linked_ptr<Entry>(new Entry(value)));
  return 1;
}

}  // namespace extensions
