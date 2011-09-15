// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_SHARED_VALUE_H_
#define CHROME_BROWSER_SYNC_UTIL_SHARED_VALUE_H_
#pragma once

#include "base/memory/ref_counted.h"

namespace browser_sync {

template <class ValueType>
struct HasSwapMemFnTraits {
  static void Swap(ValueType* t1, ValueType* t2) {
    t1->Swap(t2);
  }
};

// A ref-counted thread-safe wrapper around an immutable value.
template <class ValueType, class Traits>
class SharedValue
    : public base::RefCountedThreadSafe<SharedValue<ValueType, Traits> > {
 public:
  SharedValue() {}
  // Takes over the data in |value|, leaving |value| empty.
  explicit SharedValue(ValueType* value) {
    Traits::Swap(&value_, value);
  }

  const ValueType& Get() const {
    return value_;
  }

 private:
  ~SharedValue() {}
  friend class base::RefCountedThreadSafe<SharedValue<ValueType, Traits> >;

  ValueType value_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_SHARED_VALUE_H_
