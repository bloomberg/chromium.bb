// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/system_display_api.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/system_display/display_info_provider.h"
#include "chrome/common/extensions/api/system_display.h"
#include "chrome/common/extensions/manifest_handlers/kiosk_mode_info.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace extensions {

using api::system_display::DisplayUnitInfo;

namespace SetDisplayProperties = api::system_display::SetDisplayProperties;

typedef std::vector<linked_ptr<
    api::system_display::DisplayUnitInfo> > DisplayInfo;

bool SystemDisplayGetInfoFunction::RunImpl() {
  DisplayInfo all_displays_info =
      DisplayInfoProvider::Get()->GetAllDisplaysInfo();
  results_ = api::system_display::GetInfo::Results::Create(all_displays_info);
  return true;
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
