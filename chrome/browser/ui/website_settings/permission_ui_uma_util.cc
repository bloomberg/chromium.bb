// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_ui_uma_util.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"

void PermissionUiUmaUtil::PermissionPromptShown(
    const std::vector<PermissionBubbleRequest*>& requests) {
  PermissionBubbleType permission_prompt_type = PermissionBubbleType::MULTIPLE;
  if (requests.size() == 1)
    permission_prompt_type = requests[0]->GetPermissionBubbleType();
  UMA_HISTOGRAM_ENUMERATION(
      "Permissions.Prompt.Shown",
      static_cast<base::HistogramBase::Sample>(permission_prompt_type),
      static_cast<base::HistogramBase::Sample>(PermissionBubbleType::NUM));
}
