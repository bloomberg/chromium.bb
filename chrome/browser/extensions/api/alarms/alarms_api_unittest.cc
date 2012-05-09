// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chrome.alarms extension API.

#include "base/values.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/api/alarms/alarms_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace utils = extension_function_test_utils;

namespace extensions {

namespace {

// Test delegate which quits the message loop when an alarm fires.
class AlarmDelegate : public AlarmManager::Delegate {
 public:
  virtual ~AlarmDelegate() {}
  virtual void OnAlarm(const std::string& extension_id,
                       const AlarmManager::Alarm& alarm) {
    alarms_seen.push_back(alarm.name);
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
    system->CreateAlarmManager();
    alarm_manager_ = system->alarm_manager();

    alarm_delegate_ = new AlarmDelegate();
    alarm_manager_->set_delegate(alarm_delegate_);

    extension_ = utils::CreateEmptyExtensionWithLocation(Extension::LOAD);
  }

  base::Value* RunFunctionWithExtension(
      UIThreadExtensionFunction* function, const std::string& args) {
    function->set_extension(extension_.get());
    return utils::RunFunctionAndReturnResult(function, args, browser());
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
    return utils::RunFunctionAndReturnError(function, args, browser());
  }

  // Takes a JSON result from a function and converts it to a vector of
  // Alarms.
  AlarmManager::AlarmList ToAlarmList(base::ListValue* value) {
    AlarmManager::AlarmList list;
    for (size_t i = 0; i < value->GetSize(); ++i) {
      linked_ptr<AlarmManager::Alarm> alarm(new AlarmManager::Alarm);
      base::DictionaryValue* alarm_value;
      if (!value->GetDictionary(i, &alarm_value)) {
        ADD_FAILURE() << "Expected a list of Alarm objects.";
        return list;
      }
      EXPECT_TRUE(AlarmManager::Alarm::Populate(*alarm_value, alarm.get()));
      list.push_back(alarm);
    }
    return list;
  }

  // Creates up to 3 alarms using the extension API.
  void CreateAlarms(size_t num_alarms) {
    CHECK(num_alarms <= 3);

    const char* kCreateArgs[] = {
      "[null, {\"delayInMinutes\": 0.001, \"repeating\": true}]",
      "[\"7\", {\"delayInMinutes\": 7, \"repeating\": true}]",
      "[\"0\", {\"delayInMinutes\": 0}]"
    };
    for (size_t i = 0; i < num_alarms; ++i) {
      scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDict(
          new AlarmsCreateFunction(), kCreateArgs[i]));
      EXPECT_FALSE(result.get());
    }
  }

 protected:
  AlarmManager* alarm_manager_;
  AlarmDelegate* alarm_delegate_;
  scoped_refptr<Extension> extension_;
};

TEST_F(ExtensionAlarmsTest, Create) {
  // Create 1 non-repeating alarm.
  RunFunction(new AlarmsCreateFunction(), "[null, {\"delayInMinutes\": 0}]");

  const AlarmManager::Alarm* alarm =
      alarm_manager_->GetAlarm(extension_->id(), "");
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->name);
  EXPECT_EQ(0, alarm->delay_in_minutes);
  EXPECT_FALSE(alarm->repeating);

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
  // Create 1 repeating alarm.
  RunFunction(new AlarmsCreateFunction(),
              "[null, {\"delayInMinutes\": 0.001, \"repeating\": true}]");

  const AlarmManager::Alarm* alarm =
      alarm_manager_->GetAlarm(extension_->id(), "");
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->name);
  EXPECT_EQ(0.001, alarm->delay_in_minutes);
  EXPECT_TRUE(alarm->repeating);

  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  MessageLoop::current()->Run();

  // Wait again, and ensure the alarm fires again.
  alarm_manager_->ScheduleNextPoll(base::TimeDelta::FromSeconds(0));
  MessageLoop::current()->Run();

  ASSERT_EQ(2u, alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", alarm_delegate_->alarms_seen[0]);
}

TEST_F(ExtensionAlarmsTest, CreateDupe) {
  // Create 2 duplicate alarms. The first should be overridden.
  RunFunction(new AlarmsCreateFunction(), "[\"dup\", {\"delayInMinutes\": 1}]");
  RunFunction(new AlarmsCreateFunction(), "[\"dup\", {\"delayInMinutes\": 7}]");

  {
    const AlarmManager::AlarmList* alarms =
        alarm_manager_->GetAllAlarms(extension_->id());
    ASSERT_TRUE(alarms);
    EXPECT_EQ(1u, alarms->size());
    EXPECT_EQ(7, (*alarms)[0]->delay_in_minutes);
  }
}

TEST_F(ExtensionAlarmsTest, CreateDelayBelowMinimum) {
  // Create an alarm with delay below the minimum accepted value.
  std::string error = RunFunctionAndReturnError(new AlarmsCreateFunction(),
      "[\"negative\", {\"delayInMinutes\": -0.2}]");
  EXPECT_FALSE(error.empty());
}

TEST_F(ExtensionAlarmsTest, Get) {
  // Create 2 alarms, and make sure we can query them.
  CreateAlarms(2);

  // Get the default one.
  {
    AlarmManager::Alarm alarm;
    scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDict(
        new AlarmsGetFunction(), "[null]"));
    ASSERT_TRUE(result.get());
    EXPECT_TRUE(AlarmManager::Alarm::Populate(*result, &alarm));
    EXPECT_EQ("", alarm.name);
    EXPECT_EQ(0.001, alarm.delay_in_minutes);
    EXPECT_TRUE(alarm.repeating);
  }

  // Get "7".
  {
    AlarmManager::Alarm alarm;
    scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDict(
        new AlarmsGetFunction(), "[\"7\"]"));
    ASSERT_TRUE(result.get());
    EXPECT_TRUE(AlarmManager::Alarm::Populate(*result, &alarm));
    EXPECT_EQ("7", alarm.name);
    EXPECT_EQ(7, alarm.delay_in_minutes);
    EXPECT_TRUE(alarm.repeating);
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
    AlarmManager::AlarmList alarms = ToAlarmList(result.get());
    EXPECT_EQ(0u, alarms.size());
  }

  // Create 2 alarms, and make sure we can query them.
  CreateAlarms(2);

  {
    scoped_ptr<base::ListValue> result(RunFunctionAndReturnList(
        new AlarmsGetAllFunction(), "[null]"));
    AlarmManager::AlarmList alarms = ToAlarmList(result.get());
    EXPECT_EQ(2u, alarms.size());

    // Test the "7" alarm.
    AlarmManager::Alarm* alarm = alarms[0].get();
    if (alarm->name != "7")
      alarm = alarms[1].get();
    EXPECT_EQ("7", alarm->name);
    EXPECT_EQ(7, alarm->delay_in_minutes);
    EXPECT_TRUE(alarm->repeating);
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
    EXPECT_EQ(0.001, (*alarms)[0]->delay_in_minutes);
  }

  // Now wait for the alarms to fire, and ensure the cancelled alarms don't
  // fire.
  alarm_manager_->ScheduleNextPoll(base::TimeDelta::FromSeconds(0));
  MessageLoop::current()->Run();

  ASSERT_EQ(1u, alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", alarm_delegate_->alarms_seen[0]);

  // Ensure the 0.001-minute alarm is still there, since it's repeating.
  {
    const AlarmManager::AlarmList* alarms =
        alarm_manager_->GetAllAlarms(extension_->id());
    ASSERT_TRUE(alarms);
    EXPECT_EQ(1u, alarms->size());
    EXPECT_EQ(0.001, (*alarms)[0]->delay_in_minutes);
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
    const extensions::AlarmManager::Alarm* alarm =
        alarm_manager_->GetAlarm(extension_->id(), alarm_name);
    CHECK(alarm);
    return alarm_manager_->scheduled_times_[alarm].time;
  }
};

TEST_F(ExtensionAlarmsSchedulingTest, PollScheduling) {
  {
    RunFunction(new AlarmsCreateFunction(),
                "[\"a\", {\"delayInMinutes\": 6, \"repeating\": true}]");
    RunFunction(new AlarmsCreateFunction(),
                "[\"bb\", {\"delayInMinutes\": 8, \"repeating\": true}]");
    EXPECT_EQ(GetScheduledTime("a"), alarm_manager_->next_poll_time_);
    alarm_manager_->RemoveAllAlarms(extension_->id());
  }
  {
    RunFunction(new AlarmsCreateFunction(),
                "[\"a\", {\"delayInMinutes\": 10}]");
    RunFunction(new AlarmsCreateFunction(),
                "[\"bb\", {\"delayInMinutes\": 21}]");
    EXPECT_EQ(GetScheduledTime("a"), alarm_manager_->next_poll_time_);
    alarm_manager_->RemoveAllAlarms(extension_->id());
  }
  {
    RunFunction(new AlarmsCreateFunction(),
                "[\"a\", {\"delayInMinutes\": 10, \"repeating\": true}]");
    linked_ptr<AlarmManager::Alarm> alarm(new AlarmManager::Alarm());
    alarm->name = "bb";
    alarm->delay_in_minutes = 30;
    alarm->repeating = true;
    alarm_manager_->AddAlarmImpl(extension_->id(), alarm,
                                 base::TimeDelta::FromMinutes(3));
    EXPECT_EQ(GetScheduledTime("bb"), alarm_manager_->next_poll_time_);
    alarm_manager_->RemoveAllAlarms(extension_->id());
  }
  {
    linked_ptr<AlarmManager::Alarm> alarm(new AlarmManager::Alarm());
    alarm->name = "bb";
    alarm->delay_in_minutes = 3;
    alarm->repeating = true;
    alarm_manager_->AddAlarmImpl(extension_->id(), alarm,
                                 base::TimeDelta::FromSeconds(0));
    MessageLoop::current()->Run();
    // 5 minutes is the default minimum period (kMinPollPeriod).
    EXPECT_EQ(alarm_manager_->last_poll_time_ + base::TimeDelta::FromMinutes(5),
              alarm_manager_->next_poll_time_);
  }
}

TEST_F(ExtensionAlarmsSchedulingTest, TimerRunning) {
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
  RunFunction(new AlarmsCreateFunction(),
              "[\"a\", {\"delayInMinutes\": 0.001}]");
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  MessageLoop::current()->Run();
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
  RunFunction(new AlarmsCreateFunction(),
              "[\"bb\", {\"delayInMinutes\": 10}]");
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  alarm_manager_->RemoveAllAlarms(extension_->id());
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
}

}  // namespace extensions
