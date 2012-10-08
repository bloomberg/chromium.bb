// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notification/notification_api.h"

#include "base/bind.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/browser/extensions/extension_system.h"

namespace extensions {

NotificationShowFunction::NotificationShowFunction() {
}

NotificationShowFunction::~NotificationShowFunction() {
}

bool NotificationShowFunction::Prepare() {
  params_ = api::experimental_notification::Show::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void NotificationShowFunction::Work() {
  SetResult(Value::CreateBooleanValue(true));
}

bool NotificationShowFunction::Respond() {
  return error_.empty();
}

}  // namespace extensions
