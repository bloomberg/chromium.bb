// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_REF_COUNTED_UTIL_H__
#define CHROME_COMMON_REF_COUNTED_UTIL_H__

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

// RefCountedVector is just a vector wrapped up with
// RefCountedThreadSafe.
template<class T>
class RefCountedVector
    : public base::RefCountedThreadSafe<RefCountedVector<T> > {
 public:
  RefCountedVector() {}
  explicit RefCountedVector(const std::vector<T>& initializer)
      : data(initializer) {}

  std::vector<T> data;

 private:
  friend class base::RefCountedThreadSafe<RefCountedVector<T>>;
  ~RefCountedVector() {}

  DISALLOW_COPY_AND_ASSIGN(RefCountedVector<T>);
};

#endif  // CHROME_COMMON_REF_COUNTED_UTIL_H__
