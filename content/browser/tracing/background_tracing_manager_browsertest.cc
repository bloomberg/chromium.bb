// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/background_tracing_preemptive_config.h"
#include "content/public/browser/background_tracing_reactive_config.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace content {

class BackgroundTracingManagerBrowserTest : public ContentBrowserTest {
 public:
  BackgroundTracingManagerBrowserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingManagerBrowserTest);
};

class BackgroundTracingManagerUploadConfigWrapper {
 public:
  BackgroundTracingManagerUploadConfigWrapper(const base::Closure& callback)
      : callback_(callback), receive_count_(0) {
    receive_callback_ =
        base::Bind(&BackgroundTracingManagerUploadConfigWrapper::Upload,
                   base::Unretained(this));
  }

  void Upload(const base::RefCountedString* file_contents,
              base::Callback<void()> done_callback) {
    receive_count_ += 1;

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(done_callback));
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback_));
  }

  int get_receive_count() const { return receive_count_; }

  const BackgroundTracingManager::ReceiveCallback& get_receive_callback()
      const {
    return receive_callback_;
  }

 private:
  BackgroundTracingManager::ReceiveCallback receive_callback_;
  base::Closure callback_;
  int receive_count_;
};

void StartedFinalizingCallback(base::Closure callback,
                               bool expected,
                               bool value) {
  EXPECT_EQ(expected, value);
  if (!callback.is_null())
    callback.Run();
}

scoped_ptr<BackgroundTracingPreemptiveConfig> CreatePreemptiveConfig() {
  scoped_ptr<BackgroundTracingPreemptiveConfig> config(
      new BackgroundTracingPreemptiveConfig());

  BackgroundTracingPreemptiveConfig::MonitoringRule rule;
  rule.type =
      BackgroundTracingPreemptiveConfig::MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED;
  rule.named_trigger_info.trigger_name = "preemptive_test";

  config->configs.push_back(rule);

  return config.Pass();
}

scoped_ptr<BackgroundTracingReactiveConfig> CreateReactiveConfig() {
  scoped_ptr<BackgroundTracingReactiveConfig> config(
      new BackgroundTracingReactiveConfig());

  BackgroundTracingReactiveConfig::TracingRule rule;
  rule.type =
      BackgroundTracingReactiveConfig::TRACE_FOR_10S_OR_TRIGGER_OR_FULL;
  rule.trigger_name = "reactive_test";
  rule.category_preset =
      BackgroundTracingConfig::CategoryPreset::BENCHMARK_DEEP;

  config->configs.push_back(rule);

  return config.Pass();
}

void SetupBackgroundTracingManager() {
  content::BackgroundTracingManager::GetInstance()
      ->InvalidateTriggerHandlesForTesting();
}

void DisableScenarioWhenIdle() {
  BackgroundTracingManager::GetInstance()->SetActiveScenario(
      NULL, BackgroundTracingManager::ReceiveCallback(), false);
}

// This tests that the endpoint receives the final trace data.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceFinalContentsOnTrigger) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    scoped_ptr<BackgroundTracingPreemptiveConfig> config =
        CreatePreemptiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("preemptive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        config.Pass(), upload_config_wrapper.get_receive_callback(), true);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests triggering more than once still only gathers once.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallTriggersMoreThanOnceOnlyGatherOnce) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    scoped_ptr<BackgroundTracingPreemptiveConfig> config =
        CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        config.Pass(), upload_config_wrapper.get_receive_callback(), true);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests multiple triggers still only gathers once.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallMultipleTriggersOnlyGatherOnce) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    scoped_ptr<BackgroundTracingPreemptiveConfig> config =
        CreatePreemptiveConfig();

    BackgroundTracingPreemptiveConfig::MonitoringRule rule;
    rule.type =
        BackgroundTracingPreemptiveConfig::MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED;
    rule.named_trigger_info.trigger_name = "test1";
    config->configs.push_back(rule);

    rule.named_trigger_info.trigger_name = "test2";
    config->configs.push_back(rule);

    BackgroundTracingManager::TriggerHandle handle1 =
        BackgroundTracingManager::GetInstance()->RegisterTriggerType("test1");
    BackgroundTracingManager::TriggerHandle handle2 =
        BackgroundTracingManager::GetInstance()->RegisterTriggerType("test2");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        config.Pass(), upload_config_wrapper.get_receive_callback(), true);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle1, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle2,
        base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests that you can't trigger without a scenario set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CannotTriggerWithoutScenarioSet) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    scoped_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle,
        base::Bind(&StartedFinalizingCallback, run_loop.QuitClosure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

// This tests that no trace is triggered with a handle that isn't specified
// in the config.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DoesNotTriggerWithWrongHandle) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    scoped_ptr<BackgroundTracingPreemptiveConfig> config =
        CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "does_not_exist");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        config.Pass(), upload_config_wrapper.get_receive_callback(), true);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle,
        base::Bind(&StartedFinalizingCallback, run_loop.QuitClosure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

// This tests that no trace is triggered with an invalid handle.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DoesNotTriggerWithInvalidHandle) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    scoped_ptr<BackgroundTracingPreemptiveConfig> config =
        CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    content::BackgroundTracingManager::GetInstance()
        ->InvalidateTriggerHandlesForTesting();

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        config.Pass(), upload_config_wrapper.get_receive_callback(), true);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle,
        base::Bind(&StartedFinalizingCallback, run_loop.QuitClosure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

// This tests that preemptive mode configs will fail.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DoesNotAllowPreemptiveConfigThatsNotManual) {
  {
    SetupBackgroundTracingManager();

    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    scoped_ptr<BackgroundTracingPreemptiveConfig> config(
        new content::BackgroundTracingPreemptiveConfig());

    BackgroundTracingPreemptiveConfig::MonitoringRule rule;
    rule.type = BackgroundTracingPreemptiveConfig::
        MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE;
    rule.histogram_trigger_info.histogram_name_to_trigger_on = "fake";
    rule.histogram_trigger_info.histogram_bin_to_trigger_on = 0;
    config->configs.push_back(rule);

    bool result = BackgroundTracingManager::GetInstance()->SetActiveScenario(
        config.Pass(), upload_config_wrapper.get_receive_callback(), true);

    EXPECT_FALSE(result);
  }
}

// This tests that reactive mode records and terminates with timeout.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveTimeoutTermination) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    scoped_ptr<BackgroundTracingReactiveConfig> config =
        CreateReactiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("reactive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        config.Pass(), upload_config_wrapper.get_receive_callback(), true);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    BackgroundTracingManager::GetInstance()->FireTimerForTesting();

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests that reactive mode records and terminates with a second trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveSecondTriggerTermination) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    scoped_ptr<BackgroundTracingReactiveConfig> config =
        CreateReactiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("reactive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        config.Pass(), upload_config_wrapper.get_receive_callback(), true);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // second trigger to terminate.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests a third trigger in reactive more does not start another trace.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveThirdTriggerTimeout) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    scoped_ptr<BackgroundTracingReactiveConfig> config =
        CreateReactiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("reactive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        config.Pass(), upload_config_wrapper.get_receive_callback(), true);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // second trigger to terminate.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // third trigger to trigger again, fails as it is still gathering.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

}  // namespace content
