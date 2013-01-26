// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_display/system_info_display_api.h"

namespace extensions {

using api::system_info_display::DisplayUnitInfo;

bool SystemInfoDisplayGetDisplayInfoFunction::RunImpl() {
  DisplayInfoProvider::GetDisplayInfo()->StartQueryInfo(
      base::Bind(&SystemInfoDisplayGetDisplayInfoFunction::OnGetDisplayInfoCompleted,
                 this));
  return true;
}

void SystemInfoDisplayGetDisplayInfoFunction::OnGetDisplayInfoCompleted(
    const DisplayInfo& info, bool success) {
  if (success)
    results_ = api::system_info_display::GetDisplayInfo::Results::Create(info);
  else
    SetError("Error occurred when querying display information.");
  SendResponse(success);
}

}  // namespace extensions
