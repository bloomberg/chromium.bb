// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/background_tracing_preemptive_config.h"
#include "content/public/browser/background_tracing_reactive_config.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/zlib/zlib.h"

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

  void Upload(const scoped_refptr<base::RefCountedString>& file_contents,
              scoped_ptr<base::DictionaryValue> metadata,
              base::Callback<void()> done_callback) {
    receive_count_ += 1;
    EXPECT_TRUE(file_contents);

    size_t compressed_length = file_contents->data().length();
    const size_t kOutputBufferLength = 10 * 1024 * 1024;
    std::vector<char> output_str(kOutputBufferLength);

    z_stream stream = {0};
    stream.avail_in = compressed_length;
    stream.avail_out = kOutputBufferLength;
    stream.next_in = (Bytef*)&file_contents->data()[0];
    stream.next_out = (Bytef*)vector_as_array(&output_str);

    // 16 + MAX_WBITS means only decoding gzip encoded streams, and using
    // the biggest window size, according to zlib.h
    int result = inflateInit2(&stream, 16 + MAX_WBITS);
    EXPECT_EQ(Z_OK, result);
    result = inflate(&stream, Z_FINISH);
    int bytes_written = kOutputBufferLength - stream.avail_out;

    inflateEnd(&stream);
    EXPECT_EQ(Z_STREAM_END, result);

    last_file_contents_.assign(vector_as_array(&output_str), bytes_written);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(done_callback));
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback_));
  }

  bool TraceHasMatchingString(const char* str) {
    return last_file_contents_.find(str) != std::string::npos;
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
  std::string last_file_contents_;
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
      NULL, BackgroundTracingManager::ReceiveCallback(),
      BackgroundTracingManager::NO_DATA_FILTERING);
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
        config.Pass(), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

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
        config.Pass(), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

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

namespace {

bool IsTraceEventArgsWhitelisted(const char* category_group_name,
                                 const char* event_name) {
  if (MatchPattern(category_group_name, "benchmark") &&
      MatchPattern(event_name, "whitelisted")) {
    return true;
  }

  return false;
}

}  // namespace

// This tests that non-whitelisted args get stripped if required.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       NoWhitelistedArgsStripped) {
  SetupBackgroundTracingManager();

  base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::Bind(&IsTraceEventArgsWhitelisted));

  base::RunLoop wait_for_upload;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      wait_for_upload.QuitClosure());

  scoped_ptr<BackgroundTracingPreemptiveConfig> config =
      CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  base::RunLoop wait_for_activated;
  BackgroundTracingManager::GetInstance()->SetTracingEnabledCallbackForTesting(
      wait_for_activated.QuitClosure());
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      config.Pass(), upload_config_wrapper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  wait_for_activated.Run();

  TRACE_EVENT1("benchmark", "whitelisted", "find_this", 1);
  TRACE_EVENT1("benchmark", "not_whitelisted", "this_not_found", 1);

  BackgroundTracingManager::GetInstance()->WhenIdle(
      base::Bind(&DisableScenarioWhenIdle));

  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

  wait_for_upload.Run();

  EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("{"));
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("find_this"));
  EXPECT_TRUE(!upload_config_wrapper.TraceHasMatchingString("this_not_found"));
}

// This tests subprocesses (like a navigating renderer) which gets told to
// provide a argument-filtered trace and has no predicate in place to do the
// filtering (in this case, only the browser process gets it set), will crash
// rather than return potential PII.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CrashWhenSubprocessWithoutArgumentFilter) {
  SetupBackgroundTracingManager();

  base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::Bind(&IsTraceEventArgsWhitelisted));

  base::RunLoop wait_for_upload;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      wait_for_upload.QuitClosure());

  scoped_ptr<BackgroundTracingPreemptiveConfig> config =
      CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  base::RunLoop wait_for_activated;
  BackgroundTracingManager::GetInstance()->SetTracingEnabledCallbackForTesting(
      wait_for_activated.QuitClosure());
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      config.Pass(), upload_config_wrapper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  wait_for_activated.Run();

  NavigateToURL(shell(), GetTestUrl("", "about:blank"));

  BackgroundTracingManager::GetInstance()->WhenIdle(
      base::Bind(&DisableScenarioWhenIdle));

  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

  wait_for_upload.Run();

  EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  // We should *not* receive anything at all from the renderer,
  // the process should've crashed rather than letting that happen.
  EXPECT_TRUE(!upload_config_wrapper.TraceHasMatchingString("CrRendererMain"));
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
        config.Pass(), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

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
        config.Pass(), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

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
        config.Pass(), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

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
        config.Pass(), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

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
        config.Pass(), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

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
        config.Pass(), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

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
        config.Pass(), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

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
