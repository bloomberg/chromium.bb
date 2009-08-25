// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/perf/mem_usage.h"

#include <mach/mach.h>

#include "base/logging.h"

bool GetMemoryInfo(uint32 process_id,
                   size_t* peak_virtual_size,
                   size_t* current_virtual_size,
                   size_t* peak_working_set_size,
                   size_t* current_working_set_size) {
  NOTIMPLEMENTED();
  return false;
}

size_t GetSystemCommitCharge() {
  NOTIMPLEMENTED();
  return 0;
}

void PrintChromeMemoryUsageInfo() {
  NOTIMPLEMENTED();
}

