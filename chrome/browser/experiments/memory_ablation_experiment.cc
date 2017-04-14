// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/experiments/memory_ablation_experiment.h"

#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/alias.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_metrics.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"

const base::Feature kMemoryAblationFeature{"MemoryAblation",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const char kMemoryAblationFeatureSizeParam[] = "Size";
const char kMemoryAblationFeatureMinRAMParam[] = "MinRAM";
const char kMemoryAblationFeatureMaxRAMParam[] = "MaxRAM";

constexpr size_t kMemoryAblationDelaySeconds = 5;

MemoryAblationExperiment::MemoryAblationExperiment() {}

MemoryAblationExperiment::~MemoryAblationExperiment() {}

MemoryAblationExperiment* MemoryAblationExperiment::GetInstance() {
  static auto* instance = new MemoryAblationExperiment();
  return instance;
}

void MemoryAblationExperiment::MaybeStart(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  int min_ram_mib = base::GetFieldTrialParamByFeatureAsInt(
      kMemoryAblationFeature, kMemoryAblationFeatureMinRAMParam,
      0 /* default value */);
  int max_ram_mib = base::GetFieldTrialParamByFeatureAsInt(
      kMemoryAblationFeature, kMemoryAblationFeatureMaxRAMParam,
      std::numeric_limits<int>::max() /* default value */);
  if (base::SysInfo::AmountOfPhysicalMemoryMB() > max_ram_mib ||
      base::SysInfo::AmountOfPhysicalMemoryMB() <= min_ram_mib) {
    return;
  }

  int size = base::GetFieldTrialParamByFeatureAsInt(
      kMemoryAblationFeature, kMemoryAblationFeatureSizeParam,
      0 /* default value */);
  if (size > 0) {
    GetInstance()->Start(task_runner, size);
  }
}

void MemoryAblationExperiment::Start(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    size_t memory_size) {
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MemoryAblationExperiment::AllocateMemory,
                 base::Unretained(this), memory_size),
      base::TimeDelta::FromSeconds(kMemoryAblationDelaySeconds));
}

void MemoryAblationExperiment::AllocateMemory(size_t size) {
  memory_size_ = size;
  memory_.reset(new uint8_t[size]);
  TouchMemory();
}

void MemoryAblationExperiment::TouchMemory() {
  if (memory_) {
    size_t page_size = base::GetPageSize();
    auto* memory = static_cast<volatile uint8_t*>(memory_.get());
    for (size_t i = 0; i < memory_size_; i += page_size) {
      memory[i] = i;
    }
    base::debug::Alias(memory_.get());
  }
}
