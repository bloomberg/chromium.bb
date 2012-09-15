// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/power/power_api.h"

#include "chrome/browser/chromeos/extensions/power/power_api_manager.h"

namespace extensions {
namespace power {

bool RequestKeepAwakeFunction::RunImpl() {
  PowerApiManager* power_api_manager = PowerApiManager::GetInstance();
  power_api_manager->AddExtensionLock(extension_id());

  SetResult(base::Value::CreateBooleanValue(true));
  return true;
}

bool ReleaseKeepAwakeFunction::RunImpl() {
  PowerApiManager* power_api_manager = PowerApiManager::GetInstance();
  power_api_manager->RemoveExtensionLock(extension_id());

  SetResult(base::Value::CreateBooleanValue(true));
  return true;
}

}  // namespace power
}  // namespace extensions
