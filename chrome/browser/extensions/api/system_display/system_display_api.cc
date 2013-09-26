// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/system_display_api.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/manifest_handlers/kiosk_mode_info.h"

namespace extensions {

using api::system_display::DisplayUnitInfo;

namespace SetDisplayProperties = api::system_display::SetDisplayProperties;

bool SystemDisplayGetInfoFunction::RunImpl() {
  DisplayInfoProvider::Get()->RequestInfo(
      base::Bind(
          &SystemDisplayGetInfoFunction::OnGetDisplayInfoCompleted,
          this));
  return true;
}

void SystemDisplayGetInfoFunction::OnGetDisplayInfoCompleted(
    bool success) {
  if (success) {
    results_ = api::system_display::GetInfo::Results::Create(
                   DisplayInfoProvider::Get()->display_info());
  } else {
    SetError("Error occurred when querying display information.");
  }
  SendResponse(success);
}

bool SystemDisplaySetDisplayPropertiesFunction::RunImpl() {
#if !defined(OS_CHROMEOS)
  SetError("Function available only on ChromeOS.");
  return false;
#else
  if (!KioskModeInfo::IsKioskEnabled(GetExtension())) {
    SetError("The extension needs to be kiosk enabled to use the function.");
    return false;
  }

  scoped_ptr<SetDisplayProperties::Params> params(
      SetDisplayProperties::Params::Create(*args_));
  DisplayInfoProvider::Get()->SetInfo(params->id, params->info,
      base::Bind(
          &SystemDisplaySetDisplayPropertiesFunction::OnPropertiesSet,
          this));
  return true;
#endif
}

void SystemDisplaySetDisplayPropertiesFunction::OnPropertiesSet(
    bool success, const std::string& error) {
  if (!success)
    SetError(error);
  SendResponse(success);
}

}  // namespace extensions
