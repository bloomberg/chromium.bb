// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/test/test_api.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/api/test.h"

namespace {

// If you see this error in your test, you need to set the config state
// to be returned by chrome.test.getConfig().  Do this by calling
// TestGetConfigFunction::set_test_config_state(Value* state)
// in test set up.
const char kNoTestConfigDataError[] = "Test configuration was not set.";

const char kNotTestProcessError[] =
    "The chrome.test namespace is only available in tests.";

}  // namespace

namespace extensions {

namespace Log = api::test::Log;
namespace NotifyFail = api::test::NotifyFail;
namespace PassMessage = api::test::PassMessage;
namespace WaitForRoundTrip = api::test::WaitForRoundTrip;

TestExtensionFunction::~TestExtensionFunction() {}

bool TestExtensionFunction::RunSync() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    error_ = kNotTestProcessError;
    return false;
  }
  return RunSafe();
}

TestNotifyPassFunction::~TestNotifyPassFunction() {}

bool TestNotifyPassFunction::RunSafe() {
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_TEST_PASSED,
      content::Source<content::BrowserContext>(dispatcher()->browser_context()),
      content::NotificationService::NoDetails());
  return true;
}

TestNotifyFailFunction::~TestNotifyFailFunction() {}

bool TestNotifyFailFunction::RunSafe() {
  std::unique_ptr<NotifyFail::Params> params(
      NotifyFail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_TEST_FAILED,
      content::Source<content::BrowserContext>(dispatcher()->browser_context()),
      content::Details<std::string>(&params->message));
  return true;
}

TestLogFunction::~TestLogFunction() {}

bool TestLogFunction::RunSafe() {
  std::unique_ptr<Log::Params> params(Log::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  VLOG(1) << params->message;
  return true;
}

bool TestSendMessageFunction::RunAsync() {
  std::unique_ptr<PassMessage::Params> params(
      PassMessage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
      content::Source<TestSendMessageFunction>(this),
      content::Details<std::string>(&params->message));
  return true;
}

TestSendMessageFunction::~TestSendMessageFunction() {}

void TestSendMessageFunction::Reply(const std::string& message) {
  SetResult(new base::StringValue(message));
  SendResponse(true);
}

void TestSendMessageFunction::ReplyWithError(const std::string& error) {
  error_ = error;
  SendResponse(false);
}

// static
void TestGetConfigFunction::set_test_config_state(
    base::DictionaryValue* value) {
  TestConfigState* test_config_state = TestConfigState::GetInstance();
  test_config_state->set_config_state(value);
}

TestGetConfigFunction::TestConfigState::TestConfigState()
    : config_state_(NULL) {}

// static
TestGetConfigFunction::TestConfigState*
TestGetConfigFunction::TestConfigState::GetInstance() {
  return base::Singleton<TestConfigState>::get();
}

TestGetConfigFunction::~TestGetConfigFunction() {}

bool TestGetConfigFunction::RunSafe() {
  TestConfigState* test_config_state = TestConfigState::GetInstance();

  if (!test_config_state->config_state()) {
    error_ = kNoTestConfigDataError;
    return false;
  }

  SetResult(test_config_state->config_state()->DeepCopy());
  return true;
}

TestWaitForRoundTripFunction::~TestWaitForRoundTripFunction() {}

bool TestWaitForRoundTripFunction::RunSafe() {
  std::unique_ptr<WaitForRoundTrip::Params> params(
      WaitForRoundTrip::Params::Create(*args_));
  SetResult(new base::StringValue(params->message));
  return true;
}

}  // namespace extensions
