// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/background_tracing_rule.h"

#include <string>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/rand_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/values.h"
#include "components/tracing/tracing_messages.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kConfigRuleKey[] = "rule";
const char kConfigCategoryKey[] = "category";
const char kConfigRuleTriggerNameKey[] = "trigger_name";

const char kConfigRuleHistogramNameKey[] = "histogram_name";
const char kConfigRuleHistogramValueOldKey[] = "histogram_value";
const char kConfigRuleHistogramValue1Key[] = "histogram_lower_value";
const char kConfigRuleHistogramValue2Key[] = "histogram_upper_value";

const char kConfigRuleRandomIntervalTimeoutMin[] = "timeout_min";
const char kConfigRuleRandomIntervalTimeoutMax[] = "timeout_max";

const char kPreemptiveConfigRuleMonitorNamed[] =
    "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED";

const char kPreemptiveConfigRuleMonitorHistogram[] =
    "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE";

const char kReactiveConfigRuleTraceOnNavigationUntilTriggerOrFull[] =
    "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL";

const char kReactiveConfigRuleTraceAtRandomIntervals[] =
    "TRACE_AT_RANDOM_INTERVALS";

const char kTraceAtRandomIntervalsEventName[] =
    "ReactiveTraceAtRandomIntervals";

const int kReactiveConfigNavigationTimeout = 30;
const int kReactiveTraceRandomStartTimeMin = 60;
const int kReactiveTraceRandomStartTimeMax = 120;

}  // namespace

namespace content {

BackgroundTracingRule::BackgroundTracingRule() {}

BackgroundTracingRule::~BackgroundTracingRule() {}

bool BackgroundTracingRule::ShouldTriggerNamedEvent(
    const std::string& named_event) const {
  return false;
}

BackgroundTracingConfigImpl::CategoryPreset
BackgroundTracingRule::GetCategoryPreset() const {
  return BackgroundTracingConfigImpl::BENCHMARK;
}

namespace {

class NamedTriggerRule : public BackgroundTracingRule {
 public:
  NamedTriggerRule(const std::string& named_event)
      : named_event_(named_event) {}

  void IntoDict(base::DictionaryValue* dict) const override {
    DCHECK(dict);
    dict->SetString(kConfigRuleKey, kPreemptiveConfigRuleMonitorNamed);
    dict->SetString(kConfigRuleTriggerNameKey, named_event_.c_str());
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == named_event_;
  }

 private:
  std::string named_event_;
};

class HistogramRule : public BackgroundTracingRule,
                      public TracingControllerImpl::TraceMessageFilterObserver {
 public:
  HistogramRule(const std::string& histogram_name,
                int histogram_lower_value,
                int histogram_upper_value)
      : histogram_name_(histogram_name),
        histogram_lower_value_(histogram_lower_value),
        histogram_upper_value_(histogram_upper_value) {}

  ~HistogramRule() override {
    base::StatisticsRecorder::ClearCallback(histogram_name_);
    TracingControllerImpl::GetInstance()->RemoveTraceMessageFilterObserver(
        this);
  }

  // BackgroundTracingRule implementation
  void Install() override {
    base::StatisticsRecorder::SetCallback(
        histogram_name_,
        base::Bind(&HistogramRule::OnHistogramChangedCallback,
                   base::Unretained(this), histogram_name_,
                   histogram_lower_value_, histogram_upper_value_));

    TracingControllerImpl::GetInstance()->AddTraceMessageFilterObserver(this);
  }

  void IntoDict(base::DictionaryValue* dict) const override {
    DCHECK(dict);
    dict->SetString(kConfigRuleKey, kPreemptiveConfigRuleMonitorHistogram);
    dict->SetString(kConfigRuleHistogramNameKey, histogram_name_.c_str());
    dict->SetInteger(kConfigRuleHistogramValue1Key, histogram_lower_value_);
    dict->SetInteger(kConfigRuleHistogramValue2Key, histogram_upper_value_);
  }

  void OnHistogramTrigger(const std::string& histogram_name) const override {
    if (histogram_name != histogram_name_)
      return;

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(
            &BackgroundTracingManagerImpl::TriggerPreemptiveFinalization,
            base::Unretained(BackgroundTracingManagerImpl::GetInstance())));
  }

  // TracingControllerImpl::TraceMessageFilterObserver implementation
  void OnTraceMessageFilterAdded(TraceMessageFilter* filter) override {
    filter->Send(new TracingMsg_SetUMACallback(
        histogram_name_, histogram_lower_value_, histogram_upper_value_));
  }

  void OnTraceMessageFilterRemoved(TraceMessageFilter* filter) override {
    filter->Send(new TracingMsg_ClearUMACallback(histogram_name_));
  }

  void OnHistogramChangedCallback(const std::string& histogram_name,
                                  base::Histogram::Sample reference_lower_value,
                                  base::Histogram::Sample reference_upper_value,
                                  base::Histogram::Sample actual_value) {
    if (reference_lower_value > actual_value ||
        reference_upper_value < actual_value)
      return;

    OnHistogramTrigger(histogram_name);
  }

 private:
  std::string histogram_name_;
  int histogram_lower_value_;
  int histogram_upper_value_;
};

class ReactiveTraceForNSOrTriggerOrFullRule : public BackgroundTracingRule {
 public:
  ReactiveTraceForNSOrTriggerOrFullRule(
      const std::string& named_event,
      BackgroundTracingConfigImpl::CategoryPreset category_preset)
      : named_event_(named_event), category_preset_(category_preset) {}

  // BackgroundTracingRule implementation
  void IntoDict(base::DictionaryValue* dict) const override {
    DCHECK(dict);
    dict->SetString(
        kConfigCategoryKey,
        BackgroundTracingConfigImpl::CategoryPresetToString(category_preset_));
    dict->SetString(kConfigRuleKey,
                    kReactiveConfigRuleTraceOnNavigationUntilTriggerOrFull);
    dict->SetString(kConfigRuleTriggerNameKey, named_event_.c_str());
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == named_event_;
  }

  int GetReactiveTimeout() const override {
    return kReactiveConfigNavigationTimeout;
  }

  BackgroundTracingConfigImpl::CategoryPreset GetCategoryPreset()
      const override {
    return category_preset_;
  }

 private:
  std::string named_event_;
  BackgroundTracingConfigImpl::CategoryPreset category_preset_;
};

class ReactiveTraceAtRandomIntervalsRule : public BackgroundTracingRule {
 public:
  ReactiveTraceAtRandomIntervalsRule(
      BackgroundTracingConfigImpl::CategoryPreset category_preset,
      int timeout_min,
      int timeout_max)
      : category_preset_(category_preset),
        timeout_min_(timeout_min),
        timeout_max_(timeout_max) {
    named_event_ = GenerateUniqueName();
  }

  ~ReactiveTraceAtRandomIntervalsRule() override {}

  void IntoDict(base::DictionaryValue* dict) const override {
    DCHECK(dict);
    dict->SetString(
        kConfigCategoryKey,
        BackgroundTracingConfigImpl::CategoryPresetToString(category_preset_));
    dict->SetString(kConfigRuleKey, kReactiveConfigRuleTraceAtRandomIntervals);
    dict->SetInteger(kConfigRuleRandomIntervalTimeoutMin, timeout_min_);
    dict->SetInteger(kConfigRuleRandomIntervalTimeoutMax, timeout_max_);
  }

  void Install() override {
    handle_ = BackgroundTracingManagerImpl::GetInstance()->RegisterTriggerType(
        named_event_.c_str());

    StartTimer();
  }

  void OnStartedFinalizing(bool success) {
    if (!success)
      return;

    StartTimer();
  }

  void OnTriggerTimer() {
    BackgroundTracingManagerImpl::GetInstance()->TriggerNamedEvent(
        handle_,
        base::Bind(&ReactiveTraceAtRandomIntervalsRule::OnStartedFinalizing,
                   base::Unretained(this)));
  }

  void StartTimer() {
    int time_to_wait = base::RandInt(kReactiveTraceRandomStartTimeMin,
                                     kReactiveTraceRandomStartTimeMax);
    trigger_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(time_to_wait),
        base::Bind(&ReactiveTraceAtRandomIntervalsRule::OnTriggerTimer,
                   base::Unretained(this)));
  }

  int GetReactiveTimeout() const override {
    return base::RandInt(timeout_min_, timeout_max_);
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == named_event_;
  }

  BackgroundTracingConfigImpl::CategoryPreset GetCategoryPreset()
      const override {
    return category_preset_;
  }

  std::string GenerateUniqueName() const {
    static int ids = 0;
    char work_buffer[256];
    base::strings::SafeSNPrintf(work_buffer, sizeof(work_buffer), "%s_%d",
                                kTraceAtRandomIntervalsEventName, ids++);
    return work_buffer;
  }

 private:
  std::string named_event_;
  base::OneShotTimer<ReactiveTraceAtRandomIntervalsRule> trigger_timer_;
  BackgroundTracingConfigImpl::CategoryPreset category_preset_;
  BackgroundTracingManagerImpl::TriggerHandle handle_;
  int timeout_min_;
  int timeout_max_;
};

}  // namespace

int BackgroundTracingRule::GetReactiveTimeout() const {
  return -1;
}

scoped_ptr<BackgroundTracingRule> BackgroundTracingRule::PreemptiveRuleFromDict(
    const base::DictionaryValue* dict) {
  DCHECK(dict);

  std::string type;
  if (!dict->GetString(kConfigRuleKey, &type))
    return nullptr;

  if (type == kPreemptiveConfigRuleMonitorNamed) {
    std::string trigger_name;
    if (!dict->GetString(kConfigRuleTriggerNameKey, &trigger_name))
      return nullptr;

    return scoped_ptr<BackgroundTracingRule>(
        new NamedTriggerRule(trigger_name));
  }

  if (type == kPreemptiveConfigRuleMonitorHistogram) {
    std::string histogram_name;
    if (!dict->GetString(kConfigRuleHistogramNameKey, &histogram_name))
      return nullptr;

    // Check for the old naming.
    int histogram_value;
    if (dict->GetInteger(kConfigRuleHistogramValueOldKey, &histogram_value))
      return scoped_ptr<BackgroundTracingRule>(new HistogramRule(
          histogram_name, histogram_value, std::numeric_limits<int>::max()));

    int histogram_lower_value;
    if (!dict->GetInteger(kConfigRuleHistogramValue1Key,
                          &histogram_lower_value))
      return nullptr;

    int histogram_upper_value;
    if (!dict->GetInteger(kConfigRuleHistogramValue2Key,
                          &histogram_upper_value))
      histogram_upper_value = std::numeric_limits<int>::max();

    if (histogram_lower_value >= histogram_upper_value)
      return nullptr;

    return scoped_ptr<BackgroundTracingRule>(new HistogramRule(
        histogram_name, histogram_lower_value, histogram_upper_value));
  }

  return nullptr;
}

scoped_ptr<BackgroundTracingRule> BackgroundTracingRule::ReactiveRuleFromDict(
    const base::DictionaryValue* dict,
    BackgroundTracingConfigImpl::CategoryPreset category_preset) {
  DCHECK(dict);

  std::string type;
  if (!dict->GetString(kConfigRuleKey, &type))
    return nullptr;

  if (type == kReactiveConfigRuleTraceOnNavigationUntilTriggerOrFull) {
    std::string trigger_name;
    if (!dict->GetString(kConfigRuleTriggerNameKey, &trigger_name))
      return nullptr;

    return scoped_ptr<BackgroundTracingRule>(
        new ReactiveTraceForNSOrTriggerOrFullRule(trigger_name,
                                                  category_preset));
  }

  if (type == kReactiveConfigRuleTraceAtRandomIntervals) {
    int timeout_min;
    if (!dict->GetInteger(kConfigRuleRandomIntervalTimeoutMin, &timeout_min))
      return nullptr;

    int timeout_max;
    if (!dict->GetInteger(kConfigRuleRandomIntervalTimeoutMax, &timeout_max))
      return nullptr;

    if (timeout_min > timeout_max)
      return nullptr;

    return scoped_ptr<BackgroundTracingRule>(
        new ReactiveTraceAtRandomIntervalsRule(category_preset, timeout_min,
                                               timeout_max));
  }

  return nullptr;
}

}  // namespace content
