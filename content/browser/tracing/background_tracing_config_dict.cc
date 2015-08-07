// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/values.h"
#include "content/public/browser/background_tracing_preemptive_config.h"
#include "content/public/browser/background_tracing_reactive_config.h"

namespace content {

namespace {

const char kConfigsKey[] = "configs";

const char kConfigModeKey[] = "mode";
const char kConfigModePreemptive[] = "PREEMPTIVE_TRACING_MODE";
const char kConfigModeReactive[] = "REACTIVE_TRACING_MODE";

const char kConfigCategoryKey[] = "category";
const char kConfigCategoryBenchmark[] = "BENCHMARK";
const char kConfigCategoryBenchmarkDeep[] = "BENCHMARK_DEEP";

const char kConfigRuleKey[] = "rule";
const char kConfigRuleTriggerNameKey[] = "trigger_name";
const char kConfigRuleHistogramNameKey[] = "histogram_name";
const char kConfigRuleHistogramValueKey[] = "histogram_value";

const char kPreemptiveConfigRuleMonitorNamed[] =
    "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED";

const char kPreemptiveConfigRuleMonitorHistogram[] =
    "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE";

const char kReactiveConfigRuleTraceFor10sOrTriggerOrFull[] =
    "TRACE_FOR_10S_OR_TRIGGER_OR_FULL";

std::string CategoryPresetToString(
    BackgroundTracingConfig::CategoryPreset category_preset) {
  switch (category_preset) {
    case BackgroundTracingConfig::BENCHMARK:
      return kConfigCategoryBenchmark;
    case BackgroundTracingConfig::BENCHMARK_DEEP:
      return kConfigCategoryBenchmarkDeep;
      break;
  }
  NOTREACHED();
  return "";
}

bool StringToCategoryPreset(
    const std::string& category_preset_string,
    BackgroundTracingConfig::CategoryPreset* category_preset) {
  if (category_preset_string == kConfigCategoryBenchmark) {
    *category_preset = BackgroundTracingConfig::BENCHMARK;
    return true;
  } else if (category_preset_string == kConfigCategoryBenchmarkDeep) {
    *category_preset = BackgroundTracingConfig::BENCHMARK_DEEP;
    return true;
  }

  return false;
}

scoped_ptr<BackgroundTracingPreemptiveConfig>
BackgroundTracingPreemptiveConfig_FromDict(const base::DictionaryValue* dict) {
  DCHECK(dict);

  scoped_ptr<BackgroundTracingPreemptiveConfig> config(
      new BackgroundTracingPreemptiveConfig());

  std::string category_preset_string;
  if (!dict->GetString(kConfigCategoryKey, &category_preset_string))
    return nullptr;

  if (!StringToCategoryPreset(category_preset_string, &config->category_preset))
    return nullptr;

  const base::ListValue* configs_list = nullptr;
  if (!dict->GetList(kConfigsKey, &configs_list))
    return nullptr;

  for (const auto& it : *configs_list) {
    const base::DictionaryValue* config_dict = nullptr;
    if (!it->GetAsDictionary(&config_dict))
      return nullptr;

    std::string type;
    if (!config_dict->GetString(kConfigRuleKey, &type))
      return nullptr;

    if (type == kPreemptiveConfigRuleMonitorNamed) {
      std::string trigger_name;
      if (!config_dict->GetString(kConfigRuleTriggerNameKey, &trigger_name))
        return nullptr;

      BackgroundTracingPreemptiveConfig::MonitoringRule rule;
      rule.type = BackgroundTracingPreemptiveConfig::
          MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED;
      rule.named_trigger_info.trigger_name = trigger_name;

      config->configs.push_back(rule);
    } else if (type == kPreemptiveConfigRuleMonitorHistogram) {
      std::string histogram_name;
      if (!config_dict->GetString(kConfigRuleHistogramNameKey, &histogram_name))
        return nullptr;

      int histogram_value;
      if (!config_dict->GetInteger(kConfigRuleHistogramValueKey,
                                   &histogram_value))
        return nullptr;

      BackgroundTracingPreemptiveConfig::MonitoringRule rule;
      rule.type = BackgroundTracingPreemptiveConfig::
          MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE;
      rule.histogram_trigger_info.histogram_name = histogram_name;
      rule.histogram_trigger_info.histogram_value = histogram_value;

      config->configs.push_back(rule);
    } else {
      continue;
    }
  }

  return config.Pass();
}

scoped_ptr<BackgroundTracingReactiveConfig>
BackgroundTracingReactiveConfig_FromDict(const base::DictionaryValue* dict) {
  DCHECK(dict);

  scoped_ptr<BackgroundTracingReactiveConfig> config(
      new BackgroundTracingReactiveConfig());

  const base::ListValue* configs_list = nullptr;
  if (!dict->GetList(kConfigsKey, &configs_list))
    return nullptr;

  for (const auto& it : *configs_list) {
    const base::DictionaryValue* config_dict = nullptr;
    if (!it->GetAsDictionary(&config_dict))
      return nullptr;

    BackgroundTracingReactiveConfig::TracingRule rule;

    std::string category_preset_string;
    if (!config_dict->GetString(kConfigCategoryKey, &category_preset_string))
      return nullptr;

    if (!StringToCategoryPreset(category_preset_string, &rule.category_preset))
      return nullptr;

    std::string type;
    if (!config_dict->GetString(kConfigRuleKey, &type))
      return nullptr;

    if (type != kReactiveConfigRuleTraceFor10sOrTriggerOrFull)
      return nullptr;

    rule.type = BackgroundTracingReactiveConfig::
        TRACE_FOR_10S_OR_TRIGGER_OR_FULL;

    std::string trigger_name;
    if (!config_dict->GetString(kConfigRuleTriggerNameKey, &trigger_name))
      return nullptr;

    rule.trigger_name = trigger_name;

    config->configs.push_back(rule);
  }

  return config.Pass();
}

bool BackgroundTracingPreemptiveConfig_IntoDict(
    const BackgroundTracingPreemptiveConfig* config,
    base::DictionaryValue* dict) {
  dict->SetString(kConfigCategoryKey,
                  CategoryPresetToString(config->category_preset));

  scoped_ptr<base::ListValue> configs_list(new base::ListValue());

  for (const auto& it : config->configs) {
    scoped_ptr<base::DictionaryValue> config_dict(new base::DictionaryValue());

    if (it.type == BackgroundTracingPreemptiveConfig::
                       MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED) {
      config_dict->SetString(kConfigRuleKey, kPreemptiveConfigRuleMonitorNamed);
      config_dict->SetString(kConfigRuleTriggerNameKey,
                             it.named_trigger_info.trigger_name.c_str());
    } else if (it.type ==
               BackgroundTracingPreemptiveConfig::
                   MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE) {
      config_dict->SetString(kConfigRuleKey,
                             kPreemptiveConfigRuleMonitorHistogram);
      config_dict->SetString(kConfigRuleHistogramNameKey,
                             it.histogram_trigger_info.histogram_name.c_str());
      config_dict->SetInteger(kConfigRuleHistogramValueKey,
                              it.histogram_trigger_info.histogram_value);
    } else {
      continue;
    }

    configs_list->Append(config_dict.Pass());
  }

  dict->Set(kConfigsKey, configs_list.Pass());

  return true;
}

bool BackgroundTracingReactiveConfig_IntoDict(
    const BackgroundTracingReactiveConfig* config,
    base::DictionaryValue* dict) {
  scoped_ptr<base::ListValue> configs_list(new base::ListValue());

  for (const auto& it : config->configs) {
    scoped_ptr<base::DictionaryValue> config_dict(new base::DictionaryValue());

    config_dict->SetString(kConfigCategoryKey,
                           CategoryPresetToString(it.category_preset));

    switch (it.type) {
      case BackgroundTracingReactiveConfig::TRACE_FOR_10S_OR_TRIGGER_OR_FULL:
        config_dict->SetString(
            kConfigRuleKey,
            kReactiveConfigRuleTraceFor10sOrTriggerOrFull);
        break;
      default:
        NOTREACHED();
        continue;
    }

    config_dict->SetString(kConfigRuleTriggerNameKey, it.trigger_name.c_str());
    configs_list->Append(config_dict.Pass());
  }

  dict->Set(kConfigsKey, configs_list.Pass());

  return true;
}

}  // namespace

scoped_ptr<BackgroundTracingConfig> BackgroundTracingConfig::FromDict(
    const base::DictionaryValue* dict) {
  DCHECK(dict);

  std::string mode;
  if (!dict->GetString(kConfigModeKey, &mode))
    return nullptr;

  scoped_ptr<BackgroundTracingConfig> config;

  if (mode == kConfigModePreemptive) {
    config = BackgroundTracingPreemptiveConfig_FromDict(dict);
  } else if (mode == kConfigModeReactive) {
    config = BackgroundTracingReactiveConfig_FromDict(dict);
  } else {
    return nullptr;
  }

  return config.Pass();
}

void BackgroundTracingConfig::IntoDict(const BackgroundTracingConfig* config,
                                       base::DictionaryValue* dict) {
  switch (config->mode) {
    case PREEMPTIVE_TRACING_MODE:
      dict->SetString(kConfigModeKey, kConfigModePreemptive);
      if (!BackgroundTracingPreemptiveConfig_IntoDict(
              static_cast<const BackgroundTracingPreemptiveConfig*>(config),
              dict))
        dict->Clear();
      break;
    case REACTIVE_TRACING_MODE:
      dict->SetString(kConfigModeKey, kConfigModeReactive);
      if (!BackgroundTracingReactiveConfig_IntoDict(
              static_cast<const BackgroundTracingReactiveConfig*>(config),
              dict))
        dict->Clear();
      break;
  }
}

}  // namspace content
