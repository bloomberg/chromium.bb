// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_controls_slide_controller.h"

#if defined(OS_CHROMEOS)
#include "base/feature_list.h"
#include "chrome/browser/ui/views/frame/top_controls_slide_controller_chromeos.h"
#include "chrome/common/chrome_features.h"
#endif  // defined(OS_CHROMEOS)

std::unique_ptr<TopControlsSlideController> CreateTopControlsSlideController(
    BrowserView* browser_view) {
#if defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kSlideTopChromeWithPageScrolls))
    return std::make_unique<TopControlsSlideControllerChromeOS>(browser_view);
#endif  // defined(OS_CHROMEOS)

  return nullptr;
}
