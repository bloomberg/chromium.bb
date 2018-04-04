// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_types.h"

namespace chromeos {

IntentPickerAppInfo::IntentPickerAppInfo(AppType app_type,
                                         const gfx::Image& img,
                                         const std::string& launch,
                                         const std::string& name)
    : type(app_type), icon(img), launch_name(launch), display_name(name) {}

IntentPickerAppInfo::IntentPickerAppInfo(const IntentPickerAppInfo& app_info) =
    default;

}  // namespace chromeos
