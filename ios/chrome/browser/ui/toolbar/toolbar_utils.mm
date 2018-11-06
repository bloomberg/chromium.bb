// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_utils.h"

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns an interpolation of the height based on the multiplier associated
// with |category|, clamped between UIContentSizeCategoryLarge and
// UIContentSizeCategoryAccessibilityExtraLarge. This multiplier is applied to
// |default_height| - |non_dynamic_height|.
CGFloat Interpolate(UIContentSizeCategory category,
                    CGFloat default_height,
                    CGFloat non_dynamic_height) {
  return AlignValueToPixel(
      (default_height - non_dynamic_height) *
          SystemSuggestedFontSizeMultiplier(
              category, UIContentSizeCategoryLarge,
              UIContentSizeCategoryAccessibilityExtraLarge) +
      non_dynamic_height);
}
}  // namespace

CGFloat ToolbarCollapsedHeight(UIContentSizeCategory category) {
  return Interpolate(category, kToolbarHeightFullscreen,
                     kNonDynamicToolbarHeightFullscreen);
}

CGFloat ToolbarExpandedHeight(UIContentSizeCategory category) {
  return Interpolate(category, kAdaptiveToolbarHeight,
                     kNonDynamicToolbarHeight);
}
