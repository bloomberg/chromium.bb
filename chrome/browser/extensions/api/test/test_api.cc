// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/test/test_api.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/test.h"
#include "content/public/browser/notification_service.h"

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

namespace CreateIncognitoTab = api::test::CreateIncognitoTab;
namespace Log = api::test::Log;
namespace NotifyFail = api::test::NotifyFail;
namespace PassMessage = api::test::PassMessage;

TestExtensionFunction::~TestExtensionFunction() {}

void TestExtensionFunction::Run() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    error_ = kNotTestProcessError;
    SendResponse(false);
    return;
  }
  SendResponse(RunImpl());
}

TestNotifyPassFunction::~TestNotifyPassFunction() {}

bool TestNotifyPassFunction::RunImpl() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_TEST_PASSED,
      content::Source<Profile>(dispatcher()->profile()),
      content::NotificationService::NoDetails());
  return true;
}

TestNotifyFailFunction::~TestNotifyFailFunction() {}

bool TestNotifyFailFunction::RunImpl() {
  scoped_ptr<NotifyFail::Params> params(NotifyFail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_TEST_FAILED,
      content::Source<Profile>(dispatcher()->profile()),
      content::Details<std::string>(&params->message));
  return true;
}

TestLogFunction::~TestLogFunction() {}

bool TestLogFunction::RunImpl() {
  scoped_ptr<Log::Params> params(Log::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  VLOG(1) << params->message;
  return true;
}

TestResetQuotaFunction::~TestResetQuotaFunction() {}

bool TestResetQuotaFunction::RunImpl() {
  ExtensionService* service = GetProfile()->GetExtensionService();
  ExtensionsQuotaService* quota = service->quota_service();
  quota->Purge();
  quota->violation_errors_.clear();
  return true;
}

TestCreateIncognitoTabFunction::
   ~TestCreateIncognitoTabFunction() {}

bool TestCreateIncognitoTabFunction::RunImpl() {
  scoped_ptr<CreateIncognitoTab::Params> params(
      CreateIncognitoTab::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  chrome::OpenURLOffTheRecord(
      GetProfile(), GURL(params->url), chrome::GetActiveDesktop());
  return true;
}

bool TestSendMessageFunction::RunImpl() {
  scoped_ptr<PassMessage::Params> params(
      PassMessage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  AddRef();  // balanced in Reply
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_TEST_MESSAGE,
      content::Source<TestSendMessageFunction>(this),
      content::Details<std::string>(&params->message));
  return true;
}

TestSendMessageFunction::~TestSendMessageFunction() {}

void TestSendMessageFunction::Reply(const std::string& message) {
  SetResult(new base::StringValue(message));
  SendResponse(true);
  Release();  // balanced in RunImpl
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
  return Singleton<TestConfigState>::get();
}

TestGetConfigFunction::~TestGetConfigFunction() {}

bool TestGetConfigFunction::RunImpl() {
  TestConfigState* test_config_state = TestConfigState::GetInstance();

  if (!test_config_state->config_state()) {
    error_ = kNoTestConfigDataError;
    return false;
  }

  SetResult(test_config_state->config_state()->DeepCopy());
  return true;
}

}  // namespace extensions
