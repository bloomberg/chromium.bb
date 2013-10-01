// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/display_info_provider.h"

namespace extensions {

bool DisplayInfoProvider::SetInfo(const std::string& display_id,
    const api::system_display::DisplayProperties& info,
    std::string* error) {
  *error = "Not Implemented";
  return false;
}

void DisplayInfoProvider::UpdateDisplayUnitInfoForPlatform(
    const gfx::Display& display,
    extensions::api::system_display::DisplayUnitInfo* unit) {
  NOTIMPLEMENTED();
}

}  // namespace extensions
