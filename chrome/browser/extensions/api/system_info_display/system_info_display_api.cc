// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_display/system_info_display_api.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/manifest_handlers/kiosk_enabled_info.h"

namespace extensions {

using api::system_info_display::DisplayUnitInfo;

namespace SetDisplayProperties = api::system_info_display::SetDisplayProperties;

bool SystemInfoDisplayGetDisplayInfoFunction::RunImpl() {
  DisplayInfoProvider::GetProvider()->RequestInfo(
      base::Bind(
          &SystemInfoDisplayGetDisplayInfoFunction::OnGetDisplayInfoCompleted,
          this));
  return true;
}

void SystemInfoDisplayGetDisplayInfoFunction::OnGetDisplayInfoCompleted(
    const DisplayInfo& info, bool success) {
  if (success) {
    results_ = api::system_info_display::GetDisplayInfo::Results::Create(info);
  } else {
    SetError("Error occurred when querying display information.");
  }
  SendResponse(success);
}

bool SystemInfoDisplaySetDisplayPropertiesFunction::RunImpl() {
#if !defined(OS_CHROMEOS)
  SetError("Function available only on ChromeOS.");
  return false;
#else
  if (!KioskEnabledInfo::IsKioskEnabled(GetExtension())) {
    SetError("The extension needs to be kiosk enabled to use the function.");
    return false;
  }

  scoped_ptr<SetDisplayProperties::Params> params(
      SetDisplayProperties::Params::Create(*args_));
  DisplayInfoProvider::GetProvider()->SetInfo(params->id, params->info,
      base::Bind(
          &SystemInfoDisplaySetDisplayPropertiesFunction::OnPropertiesSet,
          this));
  return true;
#endif
}

void SystemInfoDisplaySetDisplayPropertiesFunction::OnPropertiesSet(
    bool success, const std::string& error) {
  if (!success)
    SetError(error);
  SendResponse(success);
}

}  // namespace extensions
