// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/power/power_api.h"

#include "extensions/browser/api/power/power_api_manager.h"
#include "extensions/common/api/power.h"

namespace extensions {

bool PowerRequestKeepAwakeFunction::RunSync() {
  scoped_ptr<core_api::power::RequestKeepAwake::Params> params(
      core_api::power::RequestKeepAwake::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(params->level != core_api::power::LEVEL_NONE);
  PowerApiManager::Get(browser_context())->AddRequest(
      extension_id(), params->level);
  return true;
}

bool PowerReleaseKeepAwakeFunction::RunSync() {
  PowerApiManager::Get(browser_context())->RemoveRequest(extension_id());
  return true;
}

}  // namespace extensions
