// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_info.h"

#include "base/process/process.h"
#include "base/time/time.h"

namespace base {

#if !defined(OS_ANDROID)
// static
const Time CurrentProcessInfo::CreationTime() {
  return Process::Current().CreationTime();
}
#endif  //! defined(OS_ANDROID)

}  // namespace base
