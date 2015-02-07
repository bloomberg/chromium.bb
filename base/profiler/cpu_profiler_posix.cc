// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/cpu_profiler.h"

namespace base {

CpuProfiler::CpuProfiler() {}

CpuProfiler::~CpuProfiler() {}

// static
bool CpuProfiler::IsPlatformSupported() {
  return false;
}

void CpuProfiler::OnTimer() {}

}
