// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_VALUES_UTIL_H_
#define BASE_VALUES_UTIL_H_
#pragma once

#include "base/values.h"
#include "base/ref_counted.h"

class RefCountedList : public base::RefCountedThreadSafe<RefCountedList> {
 public:
  // Takes ownership of |list|.
  explicit RefCountedList(ListValue* list);
  virtual ~RefCountedList();

  virtual ListValue* Get() {
    return list_;
  }

 private:
  ListValue* list_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedList);
};

#endif  // BASE_VALUES_UTIL_H_
