// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_STARTUP_INFO_EX_H_
#define BASE_WIN_SCOPED_STARTUP_INFO_EX_H_

#include <windows.h>

#include "base/base_export.h"
#include "base/basictypes.h"

namespace base {
namespace win {

// Manages the lifetime of additional attributes in STARTUPINFOEX.
class BASE_EXPORT ScopedStartupInfoEx {
 public:
  ScopedStartupInfoEx();

  ~ScopedStartupInfoEx();

  // Initialize the attribute list for the specified number of entries.
  bool InitializeProcThreadAttributeList(DWORD attribute_count);

  // Sets one entry in the initialized attribute list.
  bool UpdateProcThreadAttribute(DWORD_PTR attribute,
                                 void* value,
                                 size_t size);

  STARTUPINFO* startup_info() { return &startup_info_.StartupInfo; }

 private:
  STARTUPINFOEX startup_info_;
  DISALLOW_COPY_AND_ASSIGN(ScopedStartupInfoEx);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SCOPED_STARTUP_INFO_EX_H_

