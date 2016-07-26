// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/direct_memory_pressure_calculator_linux.h"

#include "base/process/process_metrics.h"
#include "base/sys_info.h"

namespace memory_pressure {

namespace {

const int kKiBperMiB = 1024;

int GetAvailableSystemMemoryMiB(const base::SystemMemoryInfoKB* mem_info) {
  // How much system memory is actively available for use right now, in MBs.
  return (mem_info->available
              ? mem_info->available
              : (mem_info->free + mem_info->buffers + mem_info->cached)) /
         kKiBperMiB;
}

}  // namespace

// Thresholds at which we consider the system being under moderate/critical
// memory pressure. They represent the percentage of system memory in use.
const int DirectMemoryPressureCalculator::kDefaultModerateThresholdPc = 70;
const int DirectMemoryPressureCalculator::kDefaultCriticalThresholdPc = 90;

DirectMemoryPressureCalculator::DirectMemoryPressureCalculator()
    : moderate_threshold_mb_(0), critical_threshold_mb_(0) {
  InferThresholds();
}

DirectMemoryPressureCalculator::DirectMemoryPressureCalculator(
    int moderate_threshold_mb,
    int critical_threshold_mb)
    : moderate_threshold_mb_(moderate_threshold_mb),
      critical_threshold_mb_(critical_threshold_mb) {
  DCHECK_GE(moderate_threshold_mb_, critical_threshold_mb_);
  DCHECK_LE(0, critical_threshold_mb_);
}

DirectMemoryPressureCalculator::MemoryPressureLevel
DirectMemoryPressureCalculator::CalculateCurrentPressureLevel() {
  base::SystemMemoryInfoKB mem_info = {};
  if (!GetSystemMemoryInfo(&mem_info))
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  int phys_free = GetAvailableSystemMemoryMiB(&mem_info);

  // Determine if the physical memory is under critical memory pressure.
  if (phys_free <= critical_threshold_mb_)
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;

  // Determine if the physical memory is under moderate memory pressure.
  if (phys_free <= moderate_threshold_mb_)
    return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;

  // No memory pressure was detected.
  return MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

bool DirectMemoryPressureCalculator::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* mem_info) const {
  return base::GetSystemMemoryInfo(mem_info);
}

void DirectMemoryPressureCalculator::InferThresholds() {
  base::SystemMemoryInfoKB mem_info = {};
  if (!GetSystemMemoryInfo(&mem_info))
    return;

  moderate_threshold_mb_ =
      mem_info.total * (100 - kDefaultModerateThresholdPc) / 100 / kKiBperMiB;
  critical_threshold_mb_ =
      mem_info.total * (100 - kDefaultCriticalThresholdPc) / 100 / kKiBperMiB;
}

}  // namespace memory_pressure
