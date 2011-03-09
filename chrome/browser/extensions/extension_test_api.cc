// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_api.h"

#include <string>

#include "base/singleton.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/common/notification_service.h"

namespace {

// If you see this error in your test, you need to set the config state
// to be returned by chrome.test.getConfig().  Do this by calling
// ExtensionTestGetConfigFunction::set_test_config_state(Value* state)
// in test set up.
const char kNoTestConfigDataError[] = "Test configuration was not set.";

}  // namespace

ExtensionTestPassFunction::~ExtensionTestPassFunction() {}

bool ExtensionTestPassFunction::RunImpl() {
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_TEST_PASSED,
      Source<Profile>(dispatcher()->profile()),
      NotificationService::NoDetails());
  return true;
}

ExtensionTestFailFunction::~ExtensionTestFailFunction() {}

bool ExtensionTestFailFunction::RunImpl() {
  std::string message;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &message));
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_TEST_FAILED,
      Source<Profile>(dispatcher()->profile()),
      Details<std::string>(&message));
  return true;
}

ExtensionTestLogFunction::~ExtensionTestLogFunction() {}

bool ExtensionTestLogFunction::RunImpl() {
  std::string message;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &message));
  return true;
}

ExtensionTestQuotaResetFunction::~ExtensionTestQuotaResetFunction() {}

bool ExtensionTestQuotaResetFunction::RunImpl() {
  ExtensionService* service = profile()->GetExtensionService();
  ExtensionsQuotaService* quota = service->quota_service();
  quota->Purge();
  quota->violators_.clear();
  return true;
}

ExtensionTestCreateIncognitoTabFunction::
   ~ExtensionTestCreateIncognitoTabFunction() {}

bool ExtensionTestCreateIncognitoTabFunction::RunImpl() {
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url));
  Browser::OpenURLOffTheRecord(profile(), GURL(url));
  return true;
}

bool ExtensionTestSendMessageFunction::RunImpl() {
  std::string message;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &message));
  AddRef();  // balanced in Reply
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_TEST_MESSAGE,
      Source<ExtensionTestSendMessageFunction>(this),
      Details<std::string>(&message));
  return true;
}
ExtensionTestSendMessageFunction::~ExtensionTestSendMessageFunction() {}

void ExtensionTestSendMessageFunction::Reply(const std::string& message) {
  result_.reset(Value::CreateStringValue(message));
  SendResponse(true);
  Release();  // balanced in RunImpl
}

// static
void ExtensionTestGetConfigFunction::set_test_config_state(
    DictionaryValue* value) {
  TestConfigState* test_config_state = TestConfigState::GetInstance();
  test_config_state->set_config_state(value);
}

ExtensionTestGetConfigFunction::TestConfigState::TestConfigState()
  : config_state_(NULL) {}

// static
ExtensionTestGetConfigFunction::TestConfigState*
ExtensionTestGetConfigFunction::TestConfigState::GetInstance() {
  return Singleton<TestConfigState>::get();
}

ExtensionTestGetConfigFunction::~ExtensionTestGetConfigFunction() {}

bool ExtensionTestGetConfigFunction::RunImpl() {
  TestConfigState* test_config_state = TestConfigState::GetInstance();

  if (!test_config_state->config_state()) {
    error_ = kNoTestConfigDataError;
    return false;
  }

  result_.reset(test_config_state->config_state()->DeepCopy());
  return true;
}
