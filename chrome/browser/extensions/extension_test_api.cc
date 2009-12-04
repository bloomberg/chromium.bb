// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/common/notification_service.h"

bool ExtensionTestPassFunction::RunImpl() {
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_TEST_PASSED,
      Source<Profile>(dispatcher()->profile()),
      NotificationService::NoDetails());
  return true;
}

bool ExtensionTestFailFunction::RunImpl() {
  std::string message;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&message));
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_TEST_FAILED,
      Source<Profile>(dispatcher()->profile()),
      Details<std::string>(&message));
  return true;
}

bool ExtensionTestLogFunction::RunImpl() {
  std::string message;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&message));
  printf("%s\n", message.c_str());
  LOG(INFO) << message;
  return true;
}

bool ExtensionTestQuotaResetFunction::RunImpl() {
  ExtensionsService* service = profile()->GetExtensionsService();
  ExtensionsQuotaService* quota = service->quota_service();
  quota->Purge();
  quota->violators_.clear();
  return true;
}
