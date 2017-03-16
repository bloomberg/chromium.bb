// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_condition_observer.h"

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
#else  // Mac, Linux, and ChromeOS
const int kDefaultExpectedRendererSizeMB = 120;
#endif

// Default values for parameters to determine the global state.
const int kDefaultNewRenderersUntilWarning = 4;
const int kDefaultNewRenderersUntilCritical = 2;
const int kDefaultNewRenderersBackToNormal = 5;
const int kDefaultNewRenderersBackToWarning = 3;
const int kDefaultMonitoringIntervalSeconds = 5;
const int kMonitoringIntervalBackgroundedSeconds = 120;

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

MemoryConditionObserver::MemoryConditionObserver(
    MemoryCoordinatorImpl* coordinator,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : coordinator_(coordinator), task_runner_(task_runner) {
  DCHECK(coordinator_);
  InitializeParameters();
  DCHECK(ValidateParameters());
}

MemoryConditionObserver::~MemoryConditionObserver() {}

void MemoryConditionObserver::ScheduleUpdateCondition(base::TimeDelta delay) {
  update_condition_closure_.Reset(base::Bind(
      &MemoryConditionObserver::UpdateCondition, base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, update_condition_closure_.callback(),
                                delay);
}

void MemoryConditionObserver::OnForegrounded() {
  SetMonitoringInterval(monitoring_interval_foregrounded_);
}

void MemoryConditionObserver::OnBackgrounded() {
  SetMonitoringInterval(monitoring_interval_backgrounded_);
}

void MemoryConditionObserver::SetMonitoringInterval(base::TimeDelta interval) {
  DCHECK(!interval.is_zero());
  if (interval == monitoring_interval_)
    return;
  monitoring_interval_ = interval;
  ScheduleUpdateCondition(interval);
}

MemoryCondition MemoryConditionObserver::CalculateNextCondition() {
  int available =
      coordinator_->memory_monitor()->GetFreeMemoryUntilCriticalMB();

  // TODO(chrisha): Move this histogram recording to a better place when
  // https://codereview.chromium.org/2479673002/ is landed.
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Coordinator.FreeMemoryUntilCritical",
                                available);

  if (available <= 0)
    return MemoryCondition::CRITICAL;

  auto current = coordinator_->GetMemoryCondition();
  int expected_renderer_count = available / expected_renderer_size_;

  switch (current) {
    case MemoryCondition::NORMAL:
      if (expected_renderer_count <= new_renderers_until_critical_)
        return MemoryCondition::CRITICAL;
      if (expected_renderer_count <= new_renderers_until_warning_)
        return MemoryCondition::WARNING;
      return MemoryCondition::NORMAL;
    case MemoryCondition::WARNING:
      if (expected_renderer_count <= new_renderers_until_critical_)
        return MemoryCondition::CRITICAL;
      if (expected_renderer_count >= new_renderers_back_to_normal_)
        return MemoryCondition::NORMAL;
      return MemoryCondition::WARNING;
    case MemoryCondition::CRITICAL:
      if (expected_renderer_count >= new_renderers_back_to_normal_)
        return MemoryCondition::NORMAL;
      if (expected_renderer_count >= new_renderers_back_to_warning_)
        return MemoryCondition::WARNING;
      return MemoryCondition::CRITICAL;
  }
  NOTREACHED();
  return MemoryCondition::NORMAL;
}

void MemoryConditionObserver::UpdateCondition() {
  auto next_condition = CalculateNextCondition();
  coordinator_->UpdateConditionIfNeeded(next_condition);
  ScheduleUpdateCondition(monitoring_interval_);
}

void MemoryConditionObserver::InitializeParameters() {
  expected_renderer_size_ = kDefaultExpectedRendererSizeMB;
  new_renderers_until_warning_ = kDefaultNewRenderersUntilWarning;
  new_renderers_until_critical_ = kDefaultNewRenderersUntilCritical;
  new_renderers_back_to_normal_ = kDefaultNewRenderersBackToNormal;
  new_renderers_back_to_warning_ = kDefaultNewRenderersBackToWarning;
  monitoring_interval_ =
      base::TimeDelta::FromSeconds(kDefaultMonitoringIntervalSeconds);
  monitoring_interval_foregrounded_ =
      base::TimeDelta::FromSeconds(kDefaultMonitoringIntervalSeconds);
  monitoring_interval_backgrounded_ =
      base::TimeDelta::FromSeconds(kMonitoringIntervalBackgroundedSeconds);

  // Override default parameters with variations.
  static constexpr char kMemoryCoordinatorV0Trial[] = "MemoryCoordinatorV0";
  std::map<std::string, std::string> params;
  variations::GetVariationParams(kMemoryCoordinatorV0Trial, &params);
  SetIntVariationParameter(params, "expected_renderer_size",
                           &expected_renderer_size_);
  SetIntVariationParameter(params, "new_renderers_until_warning",
                           &new_renderers_until_warning_);
  SetIntVariationParameter(params, "new_renderers_until_critical",
                           &new_renderers_until_critical_);
  SetIntVariationParameter(params, "new_renderers_back_to_normal",
                           &new_renderers_back_to_normal_);
  SetIntVariationParameter(params, "new_renderers_back_to_warning",
                           &new_renderers_back_to_warning_);
  SetSecondsVariationParameter(params, "monitoring_interval",
                               &monitoring_interval_);
  SetSecondsVariationParameter(params, "monitoring_interval_foregrounded",
                               &monitoring_interval_foregrounded_);
  SetSecondsVariationParameter(params, "monitoring_interval_backgrounded",
                               &monitoring_interval_backgrounded_);
}

bool MemoryConditionObserver::ValidateParameters() {
  return (new_renderers_until_warning_ > new_renderers_until_critical_) &&
         (new_renderers_back_to_normal_ > new_renderers_back_to_warning_) &&
         (new_renderers_back_to_normal_ > new_renderers_until_warning_) &&
         (new_renderers_back_to_warning_ > new_renderers_until_critical_);
}

}  // namespace content
