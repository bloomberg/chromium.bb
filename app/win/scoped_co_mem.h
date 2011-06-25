// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_WIN_SCOPED_CO_MEM_H_
#define APP_WIN_SCOPED_CO_MEM_H_
#pragma once

#include <objbase.h>

#include "base/basictypes.h"

namespace app {
namespace win {

// Simple scoped memory releaser class for COM allocated memory.
// Example:
//   app::win::ScopedCoMem<ITEMIDLIST> file_item;
//   SHGetSomeInfo(&file_item, ...);
//   ...
//   return;  <-- memory released
template<typename T>
class ScopedCoMem {
 public:
  explicit ScopedCoMem() : mem_ptr_(NULL) {}

  ~ScopedCoMem() {
    if (mem_ptr_)
      CoTaskMemFree(mem_ptr_);
  }

  T** operator&() {  // NOLINT
    return &mem_ptr_;
  }

  operator T*() {
    return mem_ptr_;
  }

 private:
  T* mem_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCoMem);
};

}  // namespace win
}  // namespace app

#endif  // APP_WIN_SCOPED_CO_MEM_H_
