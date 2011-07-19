// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SHARED_VALUE_H_
#define CHROME_BROWSER_SYNC_SHARED_VALUE_H_
#pragma once

#include "base/memory/ref_counted.h"

namespace browser_sync {

// Forward declaration for SharedValueTraits.
template <class ValueType>
class SharedValue;

// VS2005 workaround taken from base/observer_list_threadsafe.h.
template <class ValueType>
struct SharedValueTraits {
  static void Destruct(const SharedValue<ValueType>* x) {
    delete x;
  }
};

// A ref-counted thread-safe wrapper around an immutable value.
//
// ValueType should be a subclass of Value that has a Swap() method.
template <class ValueType>
class SharedValue
    : public base::RefCountedThreadSafe<
        SharedValue<ValueType>, SharedValueTraits<ValueType> > {
 public:
  SharedValue() {}
  // Takes over the data in |value|, leaving |value| empty.
  explicit SharedValue(ValueType* value) {
    value_.Swap(value);
  }

  const ValueType& Get() const {
    return value_;
  }

 private:
  ~SharedValue() {}
  friend struct SharedValueTraits<ValueType>;

  ValueType value_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SHARED_VALUE_H_
