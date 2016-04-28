// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/system_display_api.h"

#include <memory>
#include <string>

#include "build/build_config.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/common/api/system_display.h"

#if defined(OS_CHROMEOS)
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#endif

namespace extensions {

using api::system_display::DisplayUnitInfo;

namespace SetDisplayProperties = api::system_display::SetDisplayProperties;

bool SystemDisplayGetInfoFunction::RunSync() {
  DisplayUnitInfoList all_displays_info =
      DisplayInfoProvider::Get()->GetAllDisplaysInfo();
  results_ = api::system_display::GetInfo::Results::Create(all_displays_info);
  return true;
}

bool SystemDisplaySetDisplayPropertiesFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError("Function available only on ChromeOS.");
  return false;
#else
  if (extension() && !KioskModeInfo::IsKioskEnabled(extension())) {
    SetError("The extension needs to be kiosk enabled to use the function.");
    return false;
  }
  std::string error;
  std::unique_ptr<SetDisplayProperties::Params> params(
      SetDisplayProperties::Params::Create(*args_));
  bool result =
      DisplayInfoProvider::Get()->SetInfo(params->id, params->info, &error);
  if (!result)
    SetError(error);
  return result;
#endif
}

bool SystemDisplayEnableUnifiedDesktopFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError("Function available only on ChromeOS.");
  return false;
#else
  std::unique_ptr<api::system_display::EnableUnifiedDesktop::Params> params(
      api::system_display::EnableUnifiedDesktop::Params::Create(*args_));
  DisplayInfoProvider::Get()->EnableUnifiedDesktop(params->enabled);
  return true;
#endif
}

}  // namespace extensions
