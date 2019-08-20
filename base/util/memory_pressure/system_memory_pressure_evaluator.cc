// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/system_memory_pressure_evaluator.h"

#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "base/util/memory_pressure/system_memory_pressure_evaluator_chromeos.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/util/memory_pressure/system_memory_pressure_evaluator_mac.h"
#elif defined(OS_WIN)
#include "base/util/memory_pressure/system_memory_pressure_evaluator_win.h"
#endif

namespace util {

// static
std::unique_ptr<SystemMemoryPressureEvaluator>
SystemMemoryPressureEvaluator::CreateDefaultSystemEvaluator(
    MultiSourceMemoryPressureMonitor* monitor) {
#if defined(OS_CHROMEOS)
  if (util::chromeos::SystemMemoryPressureEvaluator::
          SupportsKernelNotifications()) {
    return std::make_unique<util::chromeos::SystemMemoryPressureEvaluator>(
        monitor->CreateVoter());
  }
  LOG(ERROR) << "No MemoryPressureMonitor created because the kernel does "
                "not support notifications.";
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  return std::make_unique<util::mac::SystemMemoryPressureEvaluator>(
      monitor->CreateVoter());
#elif defined(OS_WIN)
  return std::make_unique<util::win::SystemMemoryPressureEvaluator>(
      monitor->CreateVoter());
#endif
  return nullptr;
}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator() = default;

}  // namespace util
