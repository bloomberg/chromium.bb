// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/cast_display_info_provider.h"

#include "base/logging.h"

namespace extensions {

CastDisplayInfoProvider::CastDisplayInfoProvider() {}

CastDisplayInfoProvider::~CastDisplayInfoProvider() {}

bool CastDisplayInfoProvider::SetInfo(
    const std::string& display_id,
    const api::system_display::DisplayProperties& info,
    std::string* error) {
  DCHECK(error);
  *error = "Not implemented";
  return false;
}

void CastDisplayInfoProvider::UpdateDisplayUnitInfoForPlatform(
    const display::Display& display,
    extensions::api::system_display::DisplayUnitInfo* unit) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
DisplayInfoProvider* DisplayInfoProvider::Create() {
  return new CastDisplayInfoProvider();
}

}  // namespace extensions
