// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/experiments/memory_ablation_experiment.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/alias.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_metrics.h"
#include "base/sequenced_task_runner.h"

const base::Feature kMemoryAblationFeature{"MemoryAblation",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const char kMemoryAblationFeatureSizeParam[] = "Size";

constexpr size_t kMemoryAblationDelaySeconds = 5;

MemoryAblationExperiment::MemoryAblationExperiment() {}

MemoryAblationExperiment::~MemoryAblationExperiment() {}

MemoryAblationExperiment* MemoryAblationExperiment::GetInstance() {
  static auto* instance = new MemoryAblationExperiment();
  return instance;
}

void MemoryAblationExperiment::MaybeStart(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
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
