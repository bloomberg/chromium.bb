// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_config_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/values.h"
#include "content/browser/tracing/background_tracing_rule.h"

namespace content {

namespace {

const char kConfigsKey[] = "configs";

const char kConfigModeKey[] = "mode";
const char kConfigModePreemptive[] = "PREEMPTIVE_TRACING_MODE";
const char kConfigModeReactive[] = "REACTIVE_TRACING_MODE";

const char kConfigScenarioName[] = "scenario_name";
const char kConfigEnableBlinkFeatures[] = "enable_blink_features";
const char kConfigDisableBlinkFeatures[] = "disable_blink_features";

const char kConfigCategoryKey[] = "category";
const char kConfigCategoryBenchmark[] = "BENCHMARK";
const char kConfigCategoryBenchmarkDeep[] = "BENCHMARK_DEEP";
const char kConfigCategoryBenchmarkGPU[] = "BENCHMARK_GPU";
const char kConfigCategoryBenchmarkIPC[] = "BENCHMARK_IPC";
const char kConfigCategoryBenchmarkStartup[] = "BENCHMARK_STARTUP";
const char kConfigCategoryBlinkStyle[] = "BLINK_STYLE";

}  // namespace

BackgroundTracingConfigImpl::BackgroundTracingConfigImpl(
    TracingMode tracing_mode)
    : BackgroundTracingConfig(tracing_mode),
      category_preset_(BackgroundTracingConfigImpl::BENCHMARK) {}

BackgroundTracingConfigImpl::~BackgroundTracingConfigImpl() {}

std::string BackgroundTracingConfigImpl::CategoryPresetToString(
    BackgroundTracingConfigImpl::CategoryPreset category_preset) {
  switch (category_preset) {
    case BackgroundTracingConfigImpl::BENCHMARK:
      return kConfigCategoryBenchmark;
    case BackgroundTracingConfigImpl::BENCHMARK_DEEP:
      return kConfigCategoryBenchmarkDeep;
    case BackgroundTracingConfigImpl::BENCHMARK_GPU:
      return kConfigCategoryBenchmarkGPU;
    case BackgroundTracingConfigImpl::BENCHMARK_IPC:
      return kConfigCategoryBenchmarkIPC;
    case BackgroundTracingConfigImpl::BENCHMARK_STARTUP:
      return kConfigCategoryBenchmarkStartup;
    case BackgroundTracingConfigImpl::BLINK_STYLE:
      return kConfigCategoryBlinkStyle;
  }
  NOTREACHED();
  return "";
}

bool BackgroundTracingConfigImpl::StringToCategoryPreset(
    const std::string& category_preset_string,
    BackgroundTracingConfigImpl::CategoryPreset* category_preset) {
  if (category_preset_string == kConfigCategoryBenchmark) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkDeep) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_DEEP;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkGPU) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_GPU;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkIPC) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_IPC;
    return true;
  }

  if (category_preset_string == kConfigCategoryBenchmarkStartup) {
    *category_preset = BackgroundTracingConfigImpl::BENCHMARK_STARTUP;
    return true;
  }

  if (category_preset_string == kConfigCategoryBlinkStyle) {
    *category_preset = BackgroundTracingConfigImpl::BLINK_STYLE;
    return true;
  }

  return false;
}

void BackgroundTracingConfigImpl::IntoDict(base::DictionaryValue* dict) const {
  switch (tracing_mode()) {
    case BackgroundTracingConfigImpl::PREEMPTIVE:
      dict->SetString(kConfigModeKey, kConfigModePreemptive);
      dict->SetString(kConfigCategoryKey,
                      CategoryPresetToString(category_preset_));
      break;
    case BackgroundTracingConfigImpl::REACTIVE:
      dict->SetString(kConfigModeKey, kConfigModeReactive);
      break;
  }

  scoped_ptr<base::ListValue> configs_list(new base::ListValue());
  for (const auto& it : rules_) {
    scoped_ptr<base::DictionaryValue> config_dict(new base::DictionaryValue());
    DCHECK(it);
    it->IntoDict(config_dict.get());
    configs_list->Append(std::move(config_dict));
  }

  dict->Set(kConfigsKey, std::move(configs_list));

  if (!scenario_name_.empty())
    dict->SetString(kConfigScenarioName, scenario_name_);
  if (!enable_blink_features_.empty())
    dict->SetString(kConfigEnableBlinkFeatures, enable_blink_features_);
  if (!disable_blink_features_.empty())
    dict->SetString(kConfigDisableBlinkFeatures, disable_blink_features_);
}

void BackgroundTracingConfigImpl::AddPreemptiveRule(
    const base::DictionaryValue* dict) {
  scoped_ptr<BackgroundTracingRule> rule =
      BackgroundTracingRule::PreemptiveRuleFromDict(dict);
  if (rule)
    rules_.push_back(std::move(rule));
}

void BackgroundTracingConfigImpl::AddReactiveRule(
    const base::DictionaryValue* dict,
    BackgroundTracingConfigImpl::CategoryPreset category_preset) {
  scoped_ptr<BackgroundTracingRule> rule =
      BackgroundTracingRule::ReactiveRuleFromDict(dict, category_preset);
  if (rule)
    rules_.push_back(std::move(rule));
}

scoped_ptr<BackgroundTracingConfigImpl> BackgroundTracingConfigImpl::FromDict(
    const base::DictionaryValue* dict) {
  DCHECK(dict);

  std::string mode;
  if (!dict->GetString(kConfigModeKey, &mode))
    return nullptr;

  scoped_ptr<BackgroundTracingConfigImpl> config;

  if (mode == kConfigModePreemptive) {
    config = PreemptiveFromDict(dict);
  } else if (mode == kConfigModeReactive) {
    config = ReactiveFromDict(dict);
  } else {
    return nullptr;
  }

  if (config) {
    dict->GetString(kConfigScenarioName, &config->scenario_name_);
    dict->GetString(kConfigEnableBlinkFeatures,
                    &config->enable_blink_features_);
    dict->GetString(kConfigDisableBlinkFeatures,
                    &config->disable_blink_features_);
  }

  return config;
}

scoped_ptr<BackgroundTracingConfigImpl>
BackgroundTracingConfigImpl::PreemptiveFromDict(
    const base::DictionaryValue* dict) {
  DCHECK(dict);

  scoped_ptr<BackgroundTracingConfigImpl> config(
      new BackgroundTracingConfigImpl(BackgroundTracingConfigImpl::PREEMPTIVE));

  std::string category_preset_string;
  if (!dict->GetString(kConfigCategoryKey, &category_preset_string))
    return nullptr;

  if (!StringToCategoryPreset(category_preset_string,
                              &config->category_preset_))
    return nullptr;

  const base::ListValue* configs_list = nullptr;
  if (!dict->GetList(kConfigsKey, &configs_list))
    return nullptr;

  for (const auto& it : *configs_list) {
    const base::DictionaryValue* config_dict = nullptr;
    if (!it->GetAsDictionary(&config_dict))
      return nullptr;

    config->AddPreemptiveRule(config_dict);
  }

  if (config->rules().empty())
    return nullptr;

  return config;
}

scoped_ptr<BackgroundTracingConfigImpl>
BackgroundTracingConfigImpl::ReactiveFromDict(
    const base::DictionaryValue* dict) {
  DCHECK(dict);

  scoped_ptr<BackgroundTracingConfigImpl> config(
      new BackgroundTracingConfigImpl(BackgroundTracingConfigImpl::REACTIVE));

  const base::ListValue* configs_list = nullptr;
  if (!dict->GetList(kConfigsKey, &configs_list))
    return nullptr;

  for (const auto& it : *configs_list) {
    const base::DictionaryValue* config_dict = nullptr;
    if (!it->GetAsDictionary(&config_dict))
      return nullptr;

    std::string category_preset_string;
    if (!config_dict->GetString(kConfigCategoryKey, &category_preset_string))
      return nullptr;

    BackgroundTracingConfigImpl::CategoryPreset new_category_preset;
    if (!StringToCategoryPreset(category_preset_string, &new_category_preset))
      return nullptr;

    config->AddReactiveRule(config_dict, new_category_preset);
  }

  if (config->rules().empty())
    return nullptr;

  return config;
}

}  // namspace content
