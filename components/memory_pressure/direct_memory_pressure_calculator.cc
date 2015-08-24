// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/direct_memory_pressure_calculator.h"

namespace memory_pressure {

#if defined(MEMORY_PRESSURE_IS_POLLING)

bool DirectMemoryPressureCalculator::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* mem_info) const {
  return base::GetSystemMemoryInfo(mem_info);
}

#endif  // defined(MEMORY_PRESSURE_IS_POLLING)

}  // namespace memory_pressure
