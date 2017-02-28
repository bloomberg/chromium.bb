// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_state_updater.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/variations/variations_associated_data.h"
#include "content/browser/memory/memory_monitor.h"

namespace content {

namespace {

// A expected renderer size. These values come from the median of appropriate
// UMA stats.
#if defined(OS_ANDROID) || defined(OS_IOS)
const int kDefaultExpectedRendererSizeMB = 40;
#elif defined(OS_WIN)
const int kDefaultExpectedRendererSizeMB = 70;
#else // Mac, Linux, and ChromeOS
const int kDefaultExpectedRendererSizeMB = 120;
#endif

// Default values for parameters to determine the global state.
const int kDefaultNewRenderersUntilThrottled = 4;
const int kDefaultNewRenderersUntilSuspended = 2;
const int kDefaultNewRenderersBackToNormal = 5;
const int kDefaultNewRenderersBackToThrottled = 3;
const int kDefaultMinimumTransitionPeriodSeconds = 30;
const int kDefaultMonitoringIntervalSeconds = 5;

void SetIntVariationParameter(const std::map<std::string, std::string> params,
                              const char* name,
                              int* target) {
  const auto& iter = params.find(name);
  if (iter == params.end())
    return;
  int value;
  if (!iter->second.empty() && base::StringToInt(iter->second, &value)) {
    DCHECK(value > 0);
    *target = value;
  }
}

void SetSecondsVariationParameter(
    const std::map<std::string, std::string> params,
    const char* name,
    base::TimeDelta* target) {
  const auto& iter = params.find(name);
  if (iter == params.end())
    return;
  int value;
  if (!iter->second.empty() && base::StringToInt(iter->second, &value)) {
    DCHECK(value > 0);
    *target = base::TimeDelta::FromSeconds(value);
  }
}

}  // namespace

MemoryStateUpdater::MemoryStateUpdater(
    MemoryCoordinatorImpl* coordinator,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : coordinator_(coordinator), task_runner_(task_runner) {
  DCHECK(coordinator_);
  InitializeParameters();
  DCHECK(ValidateParameters());
}

MemoryStateUpdater::~MemoryStateUpdater() {}

base::MemoryState MemoryStateUpdater::CalculateNextState() {
  using MemoryState = base::MemoryState;

  int available =
      coordinator_->memory_monitor()->GetFreeMemoryUntilCriticalMB();

  // TODO(chrisha): Move this histogram recording to a better place when
  // https://codereview.chromium.org/2479673002/ is landed.
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Coordinator.FreeMemoryUntilCritical",
                                available);

  if (available <= 0)
    return MemoryState::SUSPENDED;

  auto current_state = coordinator_->GetGlobalMemoryState();
  int expected_renderer_count = available / expected_renderer_size_;

  switch (current_state) {
    case MemoryState::NORMAL:
      if (expected_renderer_count <= new_renderers_until_suspended_)
        return MemoryState::SUSPENDED;
      if (expected_renderer_count <= new_renderers_until_throttled_)
        return MemoryState::THROTTLED;
      return MemoryState::NORMAL;
    case MemoryState::THROTTLED:
      if (expected_renderer_count <= new_renderers_until_suspended_)
        return MemoryState::SUSPENDED;
      if (expected_renderer_count >= new_renderers_back_to_normal_)
        return MemoryState::NORMAL;
      return MemoryState::THROTTLED;
    case MemoryState::SUSPENDED:
      if (expected_renderer_count >= new_renderers_back_to_normal_)
        return MemoryState::NORMAL;
      if (expected_renderer_count >= new_renderers_back_to_throttled_)
        return MemoryState::THROTTLED;
      return MemoryState::SUSPENDED;
    case MemoryState::UNKNOWN:
      // Fall through
    default:
      NOTREACHED();
      return MemoryState::UNKNOWN;
  }
}

void MemoryStateUpdater::UpdateState() {
  auto current_state = coordinator_->GetGlobalMemoryState();
  auto next_state = CalculateNextState();
  if (coordinator_->ChangeStateIfNeeded(current_state, next_state)) {
    ScheduleUpdateState(minimum_transition_period_);
  } else {
    ScheduleUpdateState(monitoring_interval_);
  }
}

void MemoryStateUpdater::ScheduleUpdateState(base::TimeDelta delta) {
  update_state_closure_.Reset(base::Bind(&MemoryStateUpdater::UpdateState,
                                         base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, update_state_closure_.callback(),
                                delta);
}

void MemoryStateUpdater::InitializeParameters() {
  expected_renderer_size_ = kDefaultExpectedRendererSizeMB;
  new_renderers_until_throttled_ = kDefaultNewRenderersUntilThrottled;
  new_renderers_until_suspended_ = kDefaultNewRenderersUntilSuspended;
  new_renderers_back_to_normal_ = kDefaultNewRenderersBackToNormal;
  new_renderers_back_to_throttled_ = kDefaultNewRenderersBackToThrottled;
  minimum_transition_period_ =
      base::TimeDelta::FromSeconds(kDefaultMinimumTransitionPeriodSeconds);
  monitoring_interval_ =
      base::TimeDelta::FromSeconds(kDefaultMonitoringIntervalSeconds);

  // Override default parameters with variations.
  static constexpr char kMemoryCoordinatorV0Trial[] = "MemoryCoordinatorV0";
  std::map<std::string, std::string> params;
  variations::GetVariationParams(kMemoryCoordinatorV0Trial, &params);
  // TODO(bashi): Renaming (throttled -> warning, suspended -> critical) is
  // ongoing. Get variation parameters from both until server-side change
  // is done. crbug.com/696844
  SetIntVariationParameter(params, "expected_renderer_size",
                           &expected_renderer_size_);
  SetIntVariationParameter(params, "new_renderers_until_throttled",
                           &new_renderers_until_throttled_);
  SetIntVariationParameter(params, "new_renderers_until_warning",
                           &new_renderers_until_throttled_);
  SetIntVariationParameter(params, "new_renderers_until_suspended",
                           &new_renderers_until_suspended_);
  SetIntVariationParameter(params, "new_renderers_until_critical",
                           &new_renderers_until_suspended_);
  SetIntVariationParameter(params, "new_renderers_back_to_normal",
                           &new_renderers_back_to_normal_);
  SetIntVariationParameter(params, "new_renderers_back_to_throttled",
                           &new_renderers_back_to_throttled_);
  SetIntVariationParameter(params, "new_renderers_back_to_warning",
                           &new_renderers_back_to_throttled_);
  SetSecondsVariationParameter(params, "minimum_transition_period",
                               &minimum_transition_period_);
  SetSecondsVariationParameter(params, "monitoring_interval",
                               &monitoring_interval_);
}

bool MemoryStateUpdater::ValidateParameters() {
  return (new_renderers_until_throttled_ > new_renderers_until_suspended_) &&
      (new_renderers_back_to_normal_ > new_renderers_back_to_throttled_) &&
      (new_renderers_back_to_normal_ > new_renderers_until_throttled_) &&
      (new_renderers_back_to_throttled_ > new_renderers_until_suspended_);
}

}  // namespace content
