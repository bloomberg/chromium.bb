// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/public/browser/background_tracing_preemptive_config.h"
#include "content/public/browser/background_tracing_reactive_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BackgroundTracingConfigTest : public testing::Test {};

scoped_ptr<BackgroundTracingConfig> ReadFromJSONString(
    const std::string& json_text) {
  scoped_ptr<base::Value> json_value(base::JSONReader::Read(json_text));

  base::DictionaryValue* dict = NULL;
  if (json_value)
    json_value->GetAsDictionary(&dict);

  return content::BackgroundTracingConfig::FromDict(dict);
}

scoped_ptr<BackgroundTracingPreemptiveConfig> ReadPreemptiveFromJSONString(
    const std::string& json_text) {
  return make_scoped_ptr(static_cast<BackgroundTracingPreemptiveConfig*>(
      ReadFromJSONString(json_text).release()));
}

scoped_ptr<BackgroundTracingReactiveConfig> ReadReactiveFromJSONString(
    const std::string& json_text) {
  return make_scoped_ptr(static_cast<BackgroundTracingReactiveConfig*>(
      ReadFromJSONString(json_text).release()));
}

std::string ConfigToString(const BackgroundTracingConfig* config) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  BackgroundTracingConfig::IntoDict(config, dict.get());

  std::string results;
  if (base::JSONWriter::Write(*dict.get(), &results))
    return results;
  return "";
}

TEST_F(BackgroundTracingConfigTest, ConfigFromInvalidString) {
  // Missing or invalid mode
  EXPECT_FALSE(ReadFromJSONString("{}"));
  EXPECT_FALSE(ReadFromJSONString("{\"mode\":\"invalid\"}"));
}

TEST_F(BackgroundTracingConfigTest, PreemptiveConfigFromInvalidString) {
  // Missing or invalid category
  EXPECT_FALSE(ReadFromJSONString("{\"mode\":\"preemptive\"}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"invalid\"}"));

  // Missing or invalid configs
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\"}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": \"\"}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": {}}"));

  // Invalid config entries
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [{}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [\"invalid\"]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [[]]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [{\"rule\": \"invalid\"}]}"));

  // Missing or invalid keys for a named trigger.
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"preemptive\", \"category\": \"benchmark\","
      "\"configs\": [{\"rule\": \"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\"}]}"));
}

TEST_F(BackgroundTracingConfigTest, ReactiveConfigFromInvalidString) {
  // Missing or invalid configs
  EXPECT_FALSE(ReadFromJSONString("{\"mode\":\"reactive\"}"));
  EXPECT_FALSE(
      ReadFromJSONString("{\"mode\":\"reactive\", \"configs\": \"invalid\"}"));
  EXPECT_FALSE(ReadFromJSONString("{\"mode\":\"reactive\", \"configs\": {}}"));

  // Invalid config entries
  EXPECT_FALSE(
      ReadFromJSONString("{\"mode\":\"reactive\", \"configs\": [{}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\", \"configs\": [\"invalid\"]}"));

  // Invalid tracing rule type
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": []}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": \"\"}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"trace_for_10s_or_trigger_or_full\"}]}"));

  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"trace_for_10s_or_trigger_or_full\", \"category\": "
      "[]}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"trace_for_10s_or_trigger_or_full\", \"category\": "
      "\"\"}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"trace_for_10s_or_trigger_or_full\", \"category\": "
      "\"benchmark\"}]}"));

  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"trace_for_10s_or_trigger_or_full\", \"category\": "
      "\"benchmark\", \"trigger_name\": []}]}"));
  EXPECT_FALSE(ReadFromJSONString(
      "{\"mode\":\"reactive\","
      "\"configs\": [{\"rule\": "
      "\"trace_for_10s_or_trigger_or_full\", \"category\": "
      "\"benchmark\", \"trigger_name\": 0}]}"));
}

TEST_F(BackgroundTracingConfigTest, PreemptiveConfigFromValidString) {
  scoped_ptr<BackgroundTracingPreemptiveConfig> config;

  config = ReadPreemptiveFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK\",\"configs\": []}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->mode, BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE);
  EXPECT_EQ(config->category_preset, BackgroundTracingConfig::BENCHMARK);
  EXPECT_EQ(config->configs.size(), 0u);

  config = ReadPreemptiveFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK_DEEP\",\"configs\": []}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->mode, BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE);
  EXPECT_EQ(config->category_preset, BackgroundTracingConfig::BENCHMARK_DEEP);
  EXPECT_EQ(config->configs.size(), 0u);

  config = ReadPreemptiveFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"trigger_name\":\"foo\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->mode, BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE);
  EXPECT_EQ(config->category_preset, BackgroundTracingConfig::BENCHMARK);
  EXPECT_EQ(config->configs.size(), 1u);
  EXPECT_EQ(
      config->configs[0].type,
      BackgroundTracingPreemptiveConfig::MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED);
  EXPECT_EQ(config->configs[0].named_trigger_info.trigger_name, "foo");

  config = ReadPreemptiveFromJSONString(
      "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
      "\"BENCHMARK\",\"configs\": [{\"rule\": "
      "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", \"trigger_name\":\"foo1\"}, "
      "{\"rule\": \"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\", "
      "\"trigger_name\":\"foo2\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->mode, BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE);
  EXPECT_EQ(config->category_preset, BackgroundTracingConfig::BENCHMARK);
  EXPECT_EQ(config->configs.size(), 2u);
  EXPECT_EQ(
      config->configs[0].type,
      BackgroundTracingPreemptiveConfig::MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED);
  EXPECT_EQ(config->configs[0].named_trigger_info.trigger_name, "foo1");
  EXPECT_EQ(
      config->configs[1].type,
      BackgroundTracingPreemptiveConfig::MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED);
  EXPECT_EQ(config->configs[1].named_trigger_info.trigger_name, "foo2");
}

TEST_F(BackgroundTracingConfigTest, ReactiveConfigFromValidString) {
  scoped_ptr<BackgroundTracingReactiveConfig> config;

  config = ReadReactiveFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\", \"configs\": []}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->mode, BackgroundTracingConfig::REACTIVE_TRACING_MODE);
  EXPECT_EQ(config->configs.size(), 0u);

  config = ReadReactiveFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_FOR_10S_OR_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK\", \"trigger_name\": \"foo\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->mode, BackgroundTracingConfig::REACTIVE_TRACING_MODE);
  EXPECT_EQ(config->configs.size(), 1u);
  EXPECT_EQ(config->configs[0].type,
            BackgroundTracingReactiveConfig::TRACE_FOR_10S_OR_TRIGGER_OR_FULL);
  EXPECT_EQ(config->configs[0].trigger_name, "foo");
  EXPECT_EQ(config->configs[0].category_preset,
            BackgroundTracingConfig::BENCHMARK);

  config = ReadReactiveFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_FOR_10S_OR_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_DEEP\", \"trigger_name\": \"foo\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->mode, BackgroundTracingConfig::REACTIVE_TRACING_MODE);
  EXPECT_EQ(config->configs.size(), 1u);
  EXPECT_EQ(config->configs[0].type,
            BackgroundTracingReactiveConfig::TRACE_FOR_10S_OR_TRIGGER_OR_FULL);
  EXPECT_EQ(config->configs[0].trigger_name, "foo");
  EXPECT_EQ(config->configs[0].category_preset,
            BackgroundTracingConfig::BENCHMARK_DEEP);

  config = ReadReactiveFromJSONString(
      "{\"mode\":\"REACTIVE_TRACING_MODE\",\"configs\": [{\"rule\": "
      "\"TRACE_FOR_10S_OR_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_DEEP\", \"trigger_name\": "
      "\"foo1\"},{\"rule\": "
      "\"TRACE_FOR_10S_OR_TRIGGER_OR_FULL\", "
      "\"category\": \"BENCHMARK_DEEP\", \"trigger_name\": \"foo2\"}]}");
  EXPECT_TRUE(config);
  EXPECT_EQ(config->mode, BackgroundTracingConfig::REACTIVE_TRACING_MODE);
  EXPECT_EQ(config->configs.size(), 2u);
  EXPECT_EQ(config->configs[0].type,
            BackgroundTracingReactiveConfig::TRACE_FOR_10S_OR_TRIGGER_OR_FULL);
  EXPECT_EQ(config->configs[0].trigger_name, "foo1");
  EXPECT_EQ(config->configs[0].category_preset,
            BackgroundTracingConfig::BENCHMARK_DEEP);
  EXPECT_EQ(config->configs[1].type,
            BackgroundTracingReactiveConfig::TRACE_FOR_10S_OR_TRIGGER_OR_FULL);
  EXPECT_EQ(config->configs[1].trigger_name, "foo2");
  EXPECT_EQ(config->configs[1].category_preset,
            BackgroundTracingConfig::BENCHMARK_DEEP);
}

TEST_F(BackgroundTracingConfigTest, ValidPreemptiveConfigToString) {
  scoped_ptr<BackgroundTracingPreemptiveConfig> config(
      new BackgroundTracingPreemptiveConfig());

  // Default values
  EXPECT_EQ(ConfigToString(config.get()),
            "{\"category\":\"BENCHMARK\",\"configs\":[],\"mode\":\"PREEMPTIVE_"
            "TRACING_MODE\"}");

  // Change category_preset
  config->category_preset = BackgroundTracingConfig::BENCHMARK_DEEP;
  EXPECT_EQ(ConfigToString(config.get()),
            "{\"category\":\"BENCHMARK_DEEP\",\"configs\":[],\"mode\":"
            "\"PREEMPTIVE_TRACING_MODE\"}");

  {
    config.reset(new BackgroundTracingPreemptiveConfig());
    config->category_preset = BackgroundTracingConfig::BENCHMARK_DEEP;

    BackgroundTracingPreemptiveConfig::MonitoringRule rule;
    rule.type =
        BackgroundTracingPreemptiveConfig::MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED;
    rule.named_trigger_info.trigger_name = "foo";
    config->configs.push_back(rule);
    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK_DEEP\",\"configs\":[{\"rule\":"
              "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_name\":"
              "\"foo\"}],\"mode\":\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config.reset(new BackgroundTracingPreemptiveConfig());
    config->category_preset = BackgroundTracingConfig::BENCHMARK_DEEP;

    BackgroundTracingPreemptiveConfig::MonitoringRule rule;
    rule.type =
        BackgroundTracingPreemptiveConfig::MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED;
    rule.named_trigger_info.trigger_name = "foo1";
    config->configs.push_back(rule);
    rule.named_trigger_info.trigger_name = "foo2";
    config->configs.push_back(rule);
    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK_DEEP\",\"configs\":[{\"rule\":"
              "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_name\":"
              "\"foo1\"},{\"rule\":\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\","
              "\"trigger_name\":\"foo2\"}],\"mode\":\"PREEMPTIVE_TRACING_"
              "MODE\"}");
  }
}

TEST_F(BackgroundTracingConfigTest, InvalidPreemptiveConfigToString) {
  scoped_ptr<BackgroundTracingPreemptiveConfig> config;

  {
    config.reset(new BackgroundTracingPreemptiveConfig());

    BackgroundTracingPreemptiveConfig::MonitoringRule rule;
    rule.type = BackgroundTracingPreemptiveConfig::
        MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE;
    config->configs.push_back(rule);
    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK\",\"configs\":[],\"mode\":"
              "\"PREEMPTIVE_TRACING_MODE\"}");
  }

  {
    config.reset(new BackgroundTracingPreemptiveConfig());

    BackgroundTracingPreemptiveConfig::MonitoringRule rule;
    rule.type = BackgroundTracingPreemptiveConfig::
        MONITOR_AND_DUMP_WHEN_BROWSER_STARTUP_COMPLETE;
    config->configs.push_back(rule);
    EXPECT_EQ(ConfigToString(config.get()),
              "{\"category\":\"BENCHMARK\",\"configs\":[],\"mode\":"
              "\"PREEMPTIVE_TRACING_MODE\"}");
  }
}

TEST_F(BackgroundTracingConfigTest, ValidReactiveConfigToString) {
  scoped_ptr<BackgroundTracingReactiveConfig> config(
      new BackgroundTracingReactiveConfig());

  // Default values
  EXPECT_EQ(ConfigToString(config.get()),
            "{\"configs\":[],\"mode\":\"REACTIVE_TRACING_MODE\"}");

  {
    config.reset(new BackgroundTracingReactiveConfig());

    BackgroundTracingReactiveConfig::TracingRule rule;
    rule.type = BackgroundTracingReactiveConfig::
        TRACE_FOR_10S_OR_TRIGGER_OR_FULL;
    rule.trigger_name = "foo";
    rule.category_preset = BackgroundTracingConfig::BENCHMARK_DEEP;
    config->configs.push_back(rule);
    EXPECT_EQ(ConfigToString(config.get()),
              "{\"configs\":[{\"category\":\"BENCHMARK_DEEP\",\"rule\":\"TRACE_"
              "FOR_10S_OR_TRIGGER_OR_FULL\",\"trigger_name\":\"foo\"}],\"mode\""
              ":\"REACTIVE_TRACING_MODE\"}");
  }

  {
    config.reset(new BackgroundTracingReactiveConfig());

    BackgroundTracingReactiveConfig::TracingRule rule;
    rule.type = BackgroundTracingReactiveConfig::
        TRACE_FOR_10S_OR_TRIGGER_OR_FULL;
    rule.trigger_name = "foo1";
    rule.category_preset = BackgroundTracingConfig::BENCHMARK_DEEP;
    config->configs.push_back(rule);
    rule.trigger_name = "foo2";
    config->configs.push_back(rule);
    EXPECT_EQ(ConfigToString(config.get()),
              "{\"configs\":[{\"category\":\"BENCHMARK_DEEP\",\"rule\":\"TRACE_"
              "FOR_10S_OR_TRIGGER_OR_FULL\",\"trigger_name\":\"foo1\"},{"
              "\"category\":\"BENCHMARK_DEEP\",\"rule\":\"TRACE_FOR_10S_OR_"
              "TRIGGER_OR_FULL\",\"trigger_name\":\"foo2\"}],\"mode\":"
              "\"REACTIVE_TRACING_MODE\"}");
  }
}

}  // namspace content
