// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_stream.h"

namespace profiling {

#if defined(OS_WIN)
const wchar_t kWindowsPipePrefix[] = L"\\\\.\\pipe\\chrome_mem.";
#endif  // OS_WIN

}  // namespace profiling
