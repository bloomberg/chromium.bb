// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/power/power_api.h"

#include "chrome/browser/extensions/api/power/power_api_manager.h"
#include "chrome/common/extensions/api/power.h"

namespace extensions {

bool PowerRequestKeepAwakeFunction::RunImpl() {
  scoped_ptr<api::power::RequestKeepAwake::Params> params(
      api::power::RequestKeepAwake::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(params->level != api::power::LEVEL_NONE);
  PowerApiManager::GetInstance()->AddRequest(extension_id(), params->level);
  return true;
}

bool PowerReleaseKeepAwakeFunction::RunImpl() {
  PowerApiManager::GetInstance()->RemoveRequest(extension_id());
  return true;
}

}  // namespace extensions
