// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PERF_MEM_USAGE_H_
#define CHROME_TEST_PERF_MEM_USAGE_H_

#include "base/basictypes.h"

// Get the number of bytes currently committed by the system.
// Returns -1 on failure.
size_t GetSystemCommitCharge();

// Get and print memory usage information for running chrome processes.
void PrintChromeMemoryUsageInfo();

#endif  // CHROME_TEST_PERF_MEM_USAGE_H_
