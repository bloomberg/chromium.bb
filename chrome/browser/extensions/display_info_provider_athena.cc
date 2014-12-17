// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/display_info_provider_athena.h"

namespace extensions {

DisplayInfoProviderAthena::DisplayInfoProviderAthena() {
}

DisplayInfoProviderAthena::~DisplayInfoProviderAthena() {
}

bool DisplayInfoProviderAthena::SetInfo(
    const std::string& display_id_str,
    const core_api::system_display::DisplayProperties& info,
    std::string* error) {
  *error = "Not implemented";
  return false;
}

void DisplayInfoProviderAthena::UpdateDisplayUnitInfoForPlatform(
    const gfx::Display& display,
    extensions::core_api::system_display::DisplayUnitInfo* unit) {
  static bool logged_once = false;
  if (!logged_once) {
    NOTIMPLEMENTED();
    logged_once = true;
  }
}

gfx::Screen* DisplayInfoProviderAthena::GetActiveScreen() {
  NOTIMPLEMENTED();
  return NULL;
}

// static
DisplayInfoProvider* DisplayInfoProvider::Create() {
  return new DisplayInfoProviderAthena();
}

}  // namespace extensions
