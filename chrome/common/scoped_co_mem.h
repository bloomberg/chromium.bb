// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SCOPED_CO_MEM_H_
#define CHROME_COMMON_SCOPED_CO_MEM_H_
#pragma once

#include <objbase.h>

#include "base/basictypes.h"
#include "base/logging.h"

namespace chrome {
namespace common {

// Simple scoped memory releaser class for COM allocated memory.
// Example:
//   chrome::common::ScopedCoMem<ITEMIDLIST> file_item;
//   SHGetSomeInfo(&file_item, ...);
//   ...
//   return;  <-- memory released
template<typename T>
class ScopedCoMem {
 public:
  ScopedCoMem() : mem_ptr_(NULL) {}
  ~ScopedCoMem() {
    Reset(NULL);
  }

  T** operator&() {  // NOLINT
    DCHECK(mem_ptr_ == NULL);  // To catch memory leaks.
    return &mem_ptr_;
  }

  operator T*() {
    return mem_ptr_;
  }

  T* operator->() {
    DCHECK(mem_ptr_ != NULL);
    return mem_ptr_;
  }

  const T* operator->() const {
    DCHECK(mem_ptr_ != NULL);
    return mem_ptr_;
  }

  void Reset(T* ptr) {
    if (mem_ptr_)
      CoTaskMemFree(mem_ptr_);
    mem_ptr_ = ptr;
  }

 private:
  T* mem_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCoMem);
};

}  // namespace common
}  // namespace chrome

#endif  // CHROME_COMMON_SCOPED_CO_MEM_H_
