// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chrome.alarms extension API.

#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/api/alarms/alarms_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef extensions::api::alarms::Alarm JsAlarm;

namespace utils = extension_function_test_utils;

namespace extensions {

namespace {

// Test delegate which quits the message loop when an alarm fires.
class AlarmDelegate : public AlarmManager::Delegate {
 public:
  virtual ~AlarmDelegate() {}
  virtual void OnAlarm(const std::string& extension_id,
                       const Alarm& alarm) OVERRIDE {
    alarms_seen.push_back(alarm.js_alarm->name);
    MessageLoop::current()->Quit();
  }

  std::vector<std::string> alarms_seen;
};

}  // namespace

class ExtensionAlarmsTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();

    TestExtensionSystem* system = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(browser()->profile()));
    system->CreateAlarmManager(&test_clock_);
    alarm_manager_ = system->alarm_manager();

    alarm_delegate_ = new AlarmDelegate();
    alarm_manager_->set_delegate(alarm_delegate_);

    extension_ = utils::CreateEmptyExtensionWithLocation(
        extensions::Manifest::UNPACKED);

    // Make sure there's a RenderViewHost for alarms to warn into.
    AddTab(browser(), extension_->GetBackgroundURL());
    contents_ = browser()->tab_strip_model()->GetActiveWebContents();

    test_clock_.SetNow(base::Time::FromDoubleT(10));
  }

  base::Value* RunFunctionWithExtension(
      UIThreadExtensionFunction* function, const std::string& args) {
    scoped_refptr<UIThreadExtensionFunction> delete_function(function);
    function->set_extension(extension_.get());
    function->SetRenderViewHost(contents_->GetRenderViewHost());
    return utils::RunFunctionAndReturnSingleResult(function, args, browser());
  }

  base::DictionaryValue* RunFunctionAndReturnDict(
      UIThreadExtensionFunction* function, const std::string& args) {
    base::Value* result = RunFunctionWithExtension(function, args);
    return result ? utils::ToDictionary(result) : NULL;
  }

  base::ListValue* RunFunctionAndReturnList(
      UIThreadExtensionFunction* function, const std::string& args) {
    base::Value* result = RunFunctionWithExtension(function, args);
    return result ? utils::ToList(result) : NULL;
  }

  void RunFunction(UIThreadExtensionFunction* function,
                   const std::string& args) {
    scoped_ptr<base::Value> result(RunFunctionWithExtension(function, args));
  }

  std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                        const std::string& args) {
    function->set_extension(extension_.get());
    function->SetRenderViewHost(contents_->GetRenderViewHost());
    return utils::RunFunctionAndReturnError(function, args, browser());
  }

  void CreateAlarm(const std::string& args) {
    RunFunction(new AlarmsCreateFunction(&test_clock_), args);
  }

  // Takes a JSON result from a function and converts it to a vector of
  // JsAlarms.
  std::vector<linked_ptr<JsAlarm> > ToAlarmList(base::ListValue* value) {
    std::vector<linked_ptr<JsAlarm> > list;
    for (size_t i = 0; i < value->GetSize(); ++i) {
      linked_ptr<JsAlarm> alarm(new JsAlarm);
      base::DictionaryValue* alarm_value;
      if (!value->GetDictionary(i, &alarm_value)) {
        ADD_FAILURE() << "Expected a list of Alarm objects.";
        return list;
      }
      EXPECT_TRUE(JsAlarm::Populate(*alarm_value, alarm.get()));
      list.push_back(alarm);
    }
    return list;
  }

  // Creates up to 3 alarms using the extension API.
  void CreateAlarms(size_t num_alarms) {
    CHECK(num_alarms <= 3);

    const char* kCreateArgs[] = {
      "[null, {\"periodInMinutes\": 0.001}]",
      "[\"7\", {\"periodInMinutes\": 7}]",
      "[\"0\", {\"delayInMinutes\": 0}]",
    };
    for (size_t i = 0; i < num_alarms; ++i) {
      scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDict(
          new AlarmsCreateFunction(&test_clock_), kCreateArgs[i]));
      EXPECT_FALSE(result.get());
    }
  }

 protected:
  base::SimpleTestClock test_clock_;
  AlarmManager* alarm_manager_;
  AlarmDelegate* alarm_delegate_;
  scoped_refptr<extensions::Extension> extension_;
  content::WebContents* contents_;
};

TEST_F(ExtensionAlarmsTest, Create) {
  test_clock_.SetNow(base::Time::FromDoubleT(10));
  // Create 1 non-repeating alarm.
  CreateAlarm("[null, {\"delayInMinutes\": 0}]");

  const Alarm* alarm =
      alarm_manager_->GetAlarm(extension_->id(), "");
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10000, alarm->js_alarm->scheduled_time);
  EXPECT_FALSE(alarm->js_alarm->period_in_minutes.get());

  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  MessageLoop::current()->Run();

  ASSERT_EQ(1u, alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", alarm_delegate_->alarms_seen[0]);

  // Ensure the alarm is gone.
  {
    const AlarmManager::AlarmList* alarms =
        alarm_manager_->GetAllAlarms(extension_->id());
    ASSERT_FALSE(alarms);
  }
}

TEST_F(ExtensionAlarmsTest, CreateRepeating) {
  test_clock_.SetNow(base::Time::FromDoubleT(10));

  // Create 1 repeating alarm.
  CreateAlarm("[null, {\"periodInMinutes\": 0.001}]");

  const Alarm* alarm =
      alarm_manager_->GetAlarm(extension_->id(), "");
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10060, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes,
              testing::Pointee(testing::DoubleEq(0.001)));

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  MessageLoop::current()->Run();

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  // Wait again, and ensure the alarm fires again.
  alarm_manager_->ScheduleNextPoll();
  MessageLoop::current()->Run();

  ASSERT_EQ(2u, alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", alarm_delegate_->alarms_seen[0]);
}

TEST_F(ExtensionAlarmsTest, CreateAbsolute) {
  test_clock_.SetNow(base::Time::FromDoubleT(9.99));
  CreateAlarm("[null, {\"when\": 10001}]");

  const Alarm* alarm =
      alarm_manager_->GetAlarm(extension_->id(), "");
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10001, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes,
              testing::IsNull());

  test_clock_.SetNow(base::Time::FromDoubleT(10.1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  MessageLoop::current()->Run();

  ASSERT_FALSE(alarm_manager_->GetAlarm(extension_->id(), ""));

  ASSERT_EQ(1u, alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", alarm_delegate_->alarms_seen[0]);
}

TEST_F(ExtensionAlarmsTest, CreateRepeatingWithQuickFirstCall) {
  test_clock_.SetNow(base::Time::FromDoubleT(9.99));
  CreateAlarm("[null, {\"when\": 10001, \"periodInMinutes\": 0.001}]");

  const Alarm* alarm =
      alarm_manager_->GetAlarm(extension_->id(), "");
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10001, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes,
              testing::Pointee(testing::DoubleEq(0.001)));

  test_clock_.SetNow(base::Time::FromDoubleT(10.1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  MessageLoop::current()->Run();

  ASSERT_TRUE(alarm_manager_->GetAlarm(extension_->id(), ""));
  EXPECT_THAT(alarm_delegate_->alarms_seen, testing::ElementsAre(""));

  test_clock_.SetNow(base::Time::FromDoubleT(10.7));
  MessageLoop::current()->Run();

  ASSERT_TRUE(alarm_manager_->GetAlarm(extension_->id(), ""));
  EXPECT_THAT(alarm_delegate_->alarms_seen, testing::ElementsAre("", ""));
}

TEST_F(ExtensionAlarmsTest, CreateDupe) {
  test_clock_.SetNow(base::Time::FromDoubleT(10));

  // Create 2 duplicate alarms. The first should be overridden.
  CreateAlarm("[\"dup\", {\"delayInMinutes\": 1}]");
  CreateAlarm("[\"dup\", {\"delayInMinutes\": 7}]");

  {
    const AlarmManager::AlarmList* alarms =
        alarm_manager_->GetAllAlarms(extension_->id());
    ASSERT_TRUE(alarms);
    EXPECT_EQ(1u, alarms->size());
    EXPECT_DOUBLE_EQ(430000, (*alarms)[0].js_alarm->scheduled_time);
  }
}

TEST_F(ExtensionAlarmsTest, CreateDelayBelowMinimum) {
  // Create an alarm with delay below the minimum accepted value.
  CreateAlarm("[\"negative\", {\"delayInMinutes\": -0.2}]");
  IPC::TestSink& sink = static_cast<content::MockRenderProcessHost*>(
      contents_->GetRenderViewHost()->GetProcess())->sink();
  const IPC::Message* warning = sink.GetUniqueMessageMatching(
      ExtensionMsg_AddMessageToConsole::ID);
  ASSERT_TRUE(warning);
  content::ConsoleMessageLevel level = content::CONSOLE_MESSAGE_LEVEL_DEBUG;
  std::string message;
  ExtensionMsg_AddMessageToConsole::Read(warning, &level, &message);
  EXPECT_EQ(content::CONSOLE_MESSAGE_LEVEL_WARNING, level);
  EXPECT_THAT(message, testing::HasSubstr("delay is less than minimum of 1"));
}

TEST_F(ExtensionAlarmsTest, Get) {
  test_clock_.SetNow(base::Time::FromDoubleT(4));

  // Create 2 alarms, and make sure we can query them.
  CreateAlarms(2);

  // Get the default one.
  {
    JsAlarm alarm;
    scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDict(
        new AlarmsGetFunction(), "[null]"));
    ASSERT_TRUE(result.get());
    EXPECT_TRUE(JsAlarm::Populate(*result, &alarm));
    EXPECT_EQ("", alarm.name);
    EXPECT_DOUBLE_EQ(4060, alarm.scheduled_time);
    EXPECT_THAT(alarm.period_in_minutes,
                testing::Pointee(testing::DoubleEq(0.001)));
  }

  // Get "7".
  {
    JsAlarm alarm;
    scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDict(
        new AlarmsGetFunction(), "[\"7\"]"));
    ASSERT_TRUE(result.get());
    EXPECT_TRUE(JsAlarm::Populate(*result, &alarm));
    EXPECT_EQ("7", alarm.name);
    EXPECT_EQ(424000, alarm.scheduled_time);
    EXPECT_THAT(alarm.period_in_minutes, testing::Pointee(7));
  }

  // Get a non-existent one.
  {
    std::string error = RunFunctionAndReturnError(
        new AlarmsGetFunction(), "[\"nobody\"]");
    EXPECT_FALSE(error.empty());
  }
}

TEST_F(ExtensionAlarmsTest, GetAll) {
  // Test getAll with 0 alarms.
  {
    scoped_ptr<base::ListValue> result(RunFunctionAndReturnList(
        new AlarmsGetAllFunction(), "[]"));
    std::vector<linked_ptr<JsAlarm> > alarms = ToAlarmList(result.get());
    EXPECT_EQ(0u, alarms.size());
  }

  // Create 2 alarms, and make sure we can query them.
  CreateAlarms(2);

  {
    scoped_ptr<base::ListValue> result(RunFunctionAndReturnList(
        new AlarmsGetAllFunction(), "[null]"));
    std::vector<linked_ptr<JsAlarm> > alarms = ToAlarmList(result.get());
    EXPECT_EQ(2u, alarms.size());

    // Test the "7" alarm.
    JsAlarm* alarm = alarms[0].get();
    if (alarm->name != "7")
      alarm = alarms[1].get();
    EXPECT_EQ("7", alarm->name);
    EXPECT_THAT(alarm->period_in_minutes, testing::Pointee(7));
  }
}

TEST_F(ExtensionAlarmsTest, Clear) {
  // Clear a non-existent one.
  {
    std::string error = RunFunctionAndReturnError(
        new AlarmsClearFunction(), "[\"nobody\"]");
    EXPECT_FALSE(error.empty());
  }

  // Create 3 alarms.
  CreateAlarms(3);

  // Clear all but the 0.001-minute alarm.
  {
    RunFunction(new AlarmsClearFunction(), "[\"7\"]");
    RunFunction(new AlarmsClearFunction(), "[\"0\"]");

    const AlarmManager::AlarmList* alarms =
        alarm_manager_->GetAllAlarms(extension_->id());
    ASSERT_TRUE(alarms);
    EXPECT_EQ(1u, alarms->size());
    EXPECT_THAT((*alarms)[0].js_alarm->period_in_minutes,
                testing::Pointee(0.001));
  }

  // Now wait for the alarms to fire, and ensure the cancelled alarms don't
  // fire.
  alarm_manager_->ScheduleNextPoll();
  MessageLoop::current()->Run();

  ASSERT_EQ(1u, alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", alarm_delegate_->alarms_seen[0]);

  // Ensure the 0.001-minute alarm is still there, since it's repeating.
  {
    const AlarmManager::AlarmList* alarms =
        alarm_manager_->GetAllAlarms(extension_->id());
    ASSERT_TRUE(alarms);
    EXPECT_EQ(1u, alarms->size());
    EXPECT_THAT((*alarms)[0].js_alarm->period_in_minutes,
                testing::Pointee(0.001));
  }
}

TEST_F(ExtensionAlarmsTest, ClearAll) {
  // ClearAll with no alarms set.
  {
    scoped_ptr<base::Value> result(RunFunctionWithExtension(
        new AlarmsClearAllFunction(), "[]"));
    EXPECT_FALSE(result.get());
  }

  // Create 3 alarms.
  {
    CreateAlarms(3);
    const AlarmManager::AlarmList* alarms =
        alarm_manager_->GetAllAlarms(extension_->id());
    ASSERT_TRUE(alarms);
    EXPECT_EQ(3u, alarms->size());
  }

  // Clear them.
  {
    RunFunction(new AlarmsClearAllFunction(), "[]");
    const AlarmManager::AlarmList* alarms =
        alarm_manager_->GetAllAlarms(extension_->id());
    ASSERT_FALSE(alarms);
  }
}

class ExtensionAlarmsSchedulingTest : public ExtensionAlarmsTest {
 public:
  // Get the time that the alarm named is scheduled to run.
  base::Time GetScheduledTime(const std::string& alarm_name) {
    const extensions::Alarm* alarm =
        alarm_manager_->GetAlarm(extension_->id(), alarm_name);
    CHECK(alarm);
    return base::Time::FromJsTime(alarm->js_alarm->scheduled_time);
  }
};

TEST_F(ExtensionAlarmsSchedulingTest, PollScheduling) {
  {
    CreateAlarm("[\"a\", {\"periodInMinutes\": 6}]");
    CreateAlarm("[\"bb\", {\"periodInMinutes\": 8}]");
    EXPECT_EQ(GetScheduledTime("a"), alarm_manager_->next_poll_time_);
    alarm_manager_->RemoveAllAlarms(extension_->id());
  }
  {
    CreateAlarm("[\"a\", {\"delayInMinutes\": 10}]");
    CreateAlarm("[\"bb\", {\"delayInMinutes\": 21}]");
    EXPECT_EQ(GetScheduledTime("a"), alarm_manager_->next_poll_time_);
    alarm_manager_->RemoveAllAlarms(extension_->id());
  }
  {
    test_clock_.SetNow(base::Time::FromDoubleT(10));
    CreateAlarm("[\"a\", {\"periodInMinutes\": 10}]");
    Alarm alarm;
    alarm.js_alarm->name = "bb";
    alarm.js_alarm->scheduled_time = 30 * 60000;
    alarm.js_alarm->period_in_minutes.reset(new double(30));
    alarm_manager_->AddAlarmImpl(extension_->id(), alarm);
    EXPECT_DOUBLE_EQ(GetScheduledTime("a").ToDoubleT(),
                     alarm_manager_->next_poll_time_.ToDoubleT());
    alarm_manager_->RemoveAllAlarms(extension_->id());
  }
  {
    test_clock_.SetNow(base::Time::FromDoubleT(3 * 60 + 1));
    Alarm alarm;
    alarm.js_alarm->name = "bb";
    alarm.js_alarm->scheduled_time = 3 * 60000;
    alarm.js_alarm->period_in_minutes.reset(new double(3));
    alarm_manager_->AddAlarmImpl(extension_->id(), alarm);
    MessageLoop::current()->Run();
    EXPECT_EQ(alarm_manager_->last_poll_time_ + base::TimeDelta::FromMinutes(3),
              alarm_manager_->next_poll_time_);
    alarm_manager_->RemoveAllAlarms(extension_->id());
  }
  {
    test_clock_.SetNow(base::Time::FromDoubleT(4 * 60 + 1));
    CreateAlarm("[\"a\", {\"periodInMinutes\": 2}]");
    alarm_manager_->RemoveAlarm(extension_->id(), "a");
    Alarm alarm2;
    alarm2.js_alarm->name = "bb";
    alarm2.js_alarm->scheduled_time = 4 * 60000;
    alarm2.js_alarm->period_in_minutes.reset(new double(4));
    alarm_manager_->AddAlarmImpl(extension_->id(), alarm2);
    Alarm alarm3;
    alarm3.js_alarm->name = "ccc";
    alarm3.js_alarm->scheduled_time = 25 * 60000;
    alarm3.js_alarm->period_in_minutes.reset(new double(25));
    alarm_manager_->AddAlarmImpl(extension_->id(), alarm3);
    MessageLoop::current()->Run();
    EXPECT_EQ(alarm_manager_->last_poll_time_ + base::TimeDelta::FromMinutes(4),
              alarm_manager_->next_poll_time_);
    alarm_manager_->RemoveAllAlarms(extension_->id());
  }
}

TEST_F(ExtensionAlarmsSchedulingTest, ReleasedExtensionPollsInfrequently) {
  extension_ = utils::CreateEmptyExtensionWithLocation(
      extensions::Manifest::INTERNAL);
  test_clock_.SetNow(base::Time::FromJsTime(300000));
  CreateAlarm("[\"a\", {\"when\": 300010}]");
  CreateAlarm("[\"b\", {\"when\": 340000}]");

  // On startup (when there's no "last poll"), we let alarms fire as
  // soon as they're scheduled.
  EXPECT_DOUBLE_EQ(300010, alarm_manager_->next_poll_time_.ToJsTime());

  alarm_manager_->last_poll_time_ = base::Time::FromJsTime(290000);
  // In released extensions, we set the granularity to at least 5
  // minutes, which makes AddAlarm schedule the next poll after the
  // extension requested.
  alarm_manager_->ScheduleNextPoll();
  EXPECT_DOUBLE_EQ((alarm_manager_->last_poll_time_ +
                    base::TimeDelta::FromMinutes(1)).ToJsTime(),
                   alarm_manager_->next_poll_time_.ToJsTime());
}

TEST_F(ExtensionAlarmsSchedulingTest, TimerRunning) {
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
  CreateAlarm("[\"a\", {\"delayInMinutes\": 0.001}]");
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  MessageLoop::current()->Run();
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
  CreateAlarm("[\"bb\", {\"delayInMinutes\": 10}]");
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  alarm_manager_->RemoveAllAlarms(extension_->id());
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
}

}  // namespace extensions
