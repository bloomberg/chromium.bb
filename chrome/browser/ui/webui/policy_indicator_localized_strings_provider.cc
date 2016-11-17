// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"

#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace policy_indicator {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("controlledSettingPolicy",
                                  IDS_OPTIONS_CONTROLLED_SETTING_POLICY);
  html_source->AddLocalizedString("controlledSettingRecommendedMatches",
                                  IDS_OPTIONS_CONTROLLED_SETTING_RECOMMENDED);
  html_source->AddLocalizedString(
      "controlledSettingRecommendedDiffers",
      IDS_OPTIONS_CONTROLLED_SETTING_HAS_RECOMMENDATION);
#if defined(OS_CHROMEOS)
  html_source->AddLocalizedString("controlledSettingShared",
                                  IDS_OPTIONS_CONTROLLED_SETTING_SHARED);
  html_source->AddLocalizedString("controlledSettingOwner",
                                  IDS_OPTIONS_CONTROLLED_SETTING_OWNER);
#endif
}

}  // namespace policy_indicator
