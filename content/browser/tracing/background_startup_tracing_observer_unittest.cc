// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_startup_tracing_observer.h"

#include "build/build_config.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestPreferenceManagerImpl
    : public BackgroundStartupTracingObserver::PreferenceManager {
 public:
  void SetBackgroundStartupTracingEnabled(bool enabled) override {
    enabled_ = enabled;
  }

  bool GetBackgroundStartupTracingEnabled() const override { return enabled_; }

 private:
  bool enabled_ = false;
};

void TestStartupRuleExists(const BackgroundTracingConfigImpl& config,
                           bool exists) {
  const auto* rule =
      BackgroundStartupTracingObserver::FindStartupRuleInConfig(config);
  if (exists) {
    ASSERT_TRUE(rule);
    EXPECT_EQ(BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_STARTUP,
              rule->category_preset());
    EXPECT_EQ(30, rule->GetTraceDelay());
    EXPECT_FALSE(rule->stop_tracing_on_repeated_reactive());
  } else {
    EXPECT_FALSE(rule);
  }
}

}  // namespace

TEST(BackgroundStartupTracingObserverTest, IncludeStartupConfigIfNeeded) {
  BackgroundStartupTracingObserver* observer =
      BackgroundStartupTracingObserver::GetInstance();
  std::unique_ptr<TestPreferenceManagerImpl> test_preferences(
      new TestPreferenceManagerImpl);
  TestPreferenceManagerImpl* preferences = test_preferences.get();
  observer->SetPreferenceManagerForTesting(std::move(test_preferences));

  // Empty config without preference set should not do anything.
  std::unique_ptr<content::BackgroundTracingConfigImpl> config_impl;
  config_impl = observer->IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(config_impl);
  EXPECT_FALSE(observer->enabled_in_current_session());

  // Empty config with preference set should create a startup config, and reset
  // preference.
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
  preferences->SetBackgroundStartupTracingEnabled(true);
  config_impl = observer->IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_TRUE(observer->enabled_in_current_session());
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
  ASSERT_TRUE(config_impl);
  EXPECT_EQ(1u, config_impl->rules().size());
  EXPECT_EQ(BackgroundTracingConfig::TracingMode::REACTIVE,
            config_impl->tracing_mode());
  TestStartupRuleExists(*config_impl, true);

  // Startup config with preference set should keep config and preference same.
  preferences->SetBackgroundStartupTracingEnabled(true);
  config_impl = observer->IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_TRUE(observer->enabled_in_current_session());
  EXPECT_TRUE(preferences->GetBackgroundStartupTracingEnabled());
  ASSERT_TRUE(config_impl);
  EXPECT_EQ(1u, config_impl->rules().size());
  TestStartupRuleExists(*config_impl, true);

  // Startup config without preference set should keep config and set
  // preference.
  preferences->SetBackgroundStartupTracingEnabled(false);
  config_impl = observer->IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(observer->enabled_in_current_session());
  EXPECT_TRUE(preferences->GetBackgroundStartupTracingEnabled());
  ASSERT_TRUE(config_impl);
  EXPECT_EQ(1u, config_impl->rules().size());
  TestStartupRuleExists(*config_impl, true);

  // A GPU config without preference set should not set preference and keep
  // config same.
  std::unique_ptr<base::DictionaryValue> rules_dict(
      new base::DictionaryValue());
  rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
  rules_dict->SetString("trigger_name", "test");
  rules_dict->SetString("category", "BENCHMARK_GPU");
  base::DictionaryValue dict;
  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  rules_list->Append(std::move(rules_dict));
  dict.Set("configs", std::move(rules_list));
  config_impl = BackgroundTracingConfigImpl::ReactiveFromDict(&dict);
  ASSERT_TRUE(config_impl);

  preferences->SetBackgroundStartupTracingEnabled(false);
  config_impl = observer->IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(observer->enabled_in_current_session());
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
  EXPECT_EQ(1u, config_impl->rules().size());
  TestStartupRuleExists(*config_impl, false);

  // A GPU config with preference set should include startup config and disable
  // preference.
  preferences->SetBackgroundStartupTracingEnabled(true);
  config_impl = observer->IncludeStartupConfigIfNeeded(std::move(config_impl));
  EXPECT_TRUE(observer->enabled_in_current_session());
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
  ASSERT_TRUE(config_impl);
  EXPECT_EQ(2u, config_impl->rules().size());
  EXPECT_EQ(BackgroundTracingConfig::TracingMode::REACTIVE,
            config_impl->tracing_mode());
  TestStartupRuleExists(*config_impl, true);

  bool found_gpu = false;
  for (const auto& rule : config_impl->rules()) {
    if (rule->category_preset() ==
        BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_GPU) {
      found_gpu = true;
    }
  }
  EXPECT_TRUE(found_gpu);
  preferences->SetBackgroundStartupTracingEnabled(false);
}

}  // namespace content
