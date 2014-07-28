// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/system_display_api.h"

#include <string>

#include "chrome/browser/extensions/api/system_display/display_info_provider.h"
#include "chrome/common/extensions/api/system_display.h"

#if defined(OS_CHROMEOS)
#include "base/memory/scoped_ptr.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "ui/gfx/screen.h"
#endif

namespace extensions {

using api::system_display::DisplayUnitInfo;

namespace SetDisplayProperties = api::system_display::SetDisplayProperties;

typedef std::vector<linked_ptr<
    api::system_display::DisplayUnitInfo> > DisplayInfo;

bool SystemDisplayGetInfoFunction::RunSync() {
  DisplayInfo all_displays_info =
      DisplayInfoProvider::Get()->GetAllDisplaysInfo();
  results_ = api::system_display::GetInfo::Results::Create(all_displays_info);
  return true;
}

bool SystemDisplaySetDisplayPropertiesFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError("Function available only on ChromeOS.");
  return false;
#else
  if (!KioskModeInfo::IsKioskEnabled(extension())) {
    SetError("The extension needs to be kiosk enabled to use the function.");
    return false;
  }
  std::string error;
  scoped_ptr<SetDisplayProperties::Params> params(
      SetDisplayProperties::Params::Create(*args_));
  bool success = DisplayInfoProvider::Get()->SetInfo(params->id,
                                                     params->info,
                                                     &error);
  if (!success)
    SetError(error);
  return true;
#endif
}

}  // namespace extensions
