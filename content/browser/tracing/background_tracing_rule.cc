// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/background_tracing_rule.h"

#include <string>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
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
const char kConfigRuleHistogramValueKey[] = "histogram_value";

const char kPreemptiveConfigRuleMonitorNamed[] =
    "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED";

const char kPreemptiveConfigRuleMonitorHistogram[] =
    "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE";

const char kReactiveConfigRuleTraceFor10sOrTriggerOrFull[] =
    "TRACE_FOR_10S_OR_TRIGGER_OR_FULL";

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
  HistogramRule(const std::string& histogram_name, int histogram_value)
      : histogram_name_(histogram_name), histogram_value_(histogram_value) {}

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
                   base::Unretained(this), histogram_name_, histogram_value_));

    TracingControllerImpl::GetInstance()->AddTraceMessageFilterObserver(this);
  }

  void IntoDict(base::DictionaryValue* dict) const override {
    DCHECK(dict);
    dict->SetString(kConfigRuleKey, kPreemptiveConfigRuleMonitorHistogram);
    dict->SetString(kConfigRuleHistogramNameKey, histogram_name_.c_str());
    dict->SetInteger(kConfigRuleHistogramValueKey, histogram_value_);
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
    filter->Send(
        new TracingMsg_SetUMACallback(histogram_name_, histogram_value_));
  }

  void OnTraceMessageFilterRemoved(TraceMessageFilter* filter) override {
    filter->Send(new TracingMsg_ClearUMACallback(histogram_name_));
  }

  void OnHistogramChangedCallback(const std::string& histogram_name,
                                  base::Histogram::Sample reference_value,
                                  base::Histogram::Sample actual_value) {
    if (reference_value > actual_value)
      return;

    OnHistogramTrigger(histogram_name);
  }

 private:
  std::string histogram_name_;
  int histogram_value_;
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
                    kReactiveConfigRuleTraceFor10sOrTriggerOrFull);
    dict->SetString(kConfigRuleTriggerNameKey, named_event_.c_str());
  }

  bool ShouldTriggerNamedEvent(const std::string& named_event) const override {
    return named_event == named_event_;
  }

  BackgroundTracingConfigImpl::CategoryPreset GetCategoryPreset()
      const override {
    return category_preset_;
  }

 private:
  std::string named_event_;
  BackgroundTracingConfigImpl::CategoryPreset category_preset_;
};

}  // namespace

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

    int histogram_value;
    if (!dict->GetInteger(kConfigRuleHistogramValueKey, &histogram_value))
      return nullptr;

    return scoped_ptr<BackgroundTracingRule>(
        new HistogramRule(histogram_name, histogram_value));
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

  if (type != kReactiveConfigRuleTraceFor10sOrTriggerOrFull)
    return nullptr;

  std::string trigger_name;
  if (!dict->GetString(kConfigRuleTriggerNameKey, &trigger_name))
    return nullptr;

  return scoped_ptr<BackgroundTracingRule>(
      new ReactiveTraceForNSOrTriggerOrFullRule(trigger_name, category_preset));
}

}  // namespace content
