// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/child/v8_breakpad_support_win.h"

#include <windows.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "components/crash/content/app/crash_export_thunks.h"
#include "gin/public/debug.h"

namespace v8_breakpad_support {

void SetUp() {
#if defined(ARCH_CPU_X86_64)
  gin::Debug::SetCodeRangeCreatedCallback(
      &RegisterNonABICompliantCodeRange_ExportThunk);
  gin::Debug::SetCodeRangeDeletedCallback(
      &UnregisterNonABICompliantCodeRange_ExportThunk);
#endif
}

}  // namespace v8_breakpad_support
