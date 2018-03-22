// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/layout_constants.h"

#include "base/logging.h"
#include "ui/base/material_design/material_design_controller.h"

int GetLayoutConstant(LayoutConstant constant) {
  const int mode = ui::MaterialDesignController::GetMode();
  const bool hybrid = mode == ui::MaterialDesignController::MATERIAL_HYBRID;
  const bool touch_optimized_material =
      mode == ui::MaterialDesignController::MATERIAL_TOUCH_OPTIMIZED;
  switch (constant) {
    case BOOKMARK_BAR_HEIGHT:
      return touch_optimized_material ? 40 : 28;
#if defined(OS_MACOSX)
    case BOOKMARK_BAR_HEIGHT_NO_OVERLAP:
      return GetLayoutConstant(BOOKMARK_BAR_HEIGHT) - 2;
#endif
    case BOOKMARK_BAR_NTP_HEIGHT:
      return touch_optimized_material ? GetLayoutConstant(BOOKMARK_BAR_HEIGHT)
                                      : 39;
#if defined(OS_MACOSX)
    case BOOKMARK_BAR_NTP_PADDING:
      return (GetLayoutConstant(BOOKMARK_BAR_NTP_HEIGHT) -
              GetLayoutConstant(BOOKMARK_BAR_HEIGHT)) /
             2;
#endif
    case LOCATION_BAR_BUBBLE_VERTICAL_PADDING:
      return hybrid ? 1 : 3;
    case LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING:
      return hybrid ? 3 : 2;
    case LOCATION_BAR_BUBBLE_CORNER_RADIUS:
      // TODO(tapted): This should match BubbleBorder::GetBorderRadius() once
      // MD is default for secondary UI everywhere. That is, the constant should
      // move to views/layout_provider.h so that all bubbles are consistent.
      return touch_optimized_material ? 8 : 2;
    case LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET:
      if (ui::MaterialDesignController::IsSecondaryUiMaterial())
        return 1;
      return hybrid ? 8 : 6;
    case LOCATION_BAR_ELEMENT_PADDING:
      return hybrid ? 3 : 1;
    case LOCATION_BAR_PADDING:
      return hybrid ? 3 : 1;
    case LOCATION_BAR_HEIGHT: {
      constexpr int kHeights[] = {28, 32, 36};
      return kHeights[mode];
    }
    case LOCATION_BAR_ICON_SIZE:
      return 16;
    case LOCATION_BAR_ICON_INTERIOR_PADDING:
      return touch_optimized_material ? 8 : 4;
    case TABSTRIP_NEW_TAB_BUTTON_SPACING: {
      // In non-touch optimized UI, we make the new tab button overlap with the
      // last tab in the tabstrip (i.e negative spacing). However, in
      // touch-optimized UI, we actually want to push the new tab button
      // further away from the tab. The distance is 8 DIP from the point at
      // which the last tab's endcap intersects with the tabstrip separator,
      // which is actually 6 DIP from the last tab's right point.
      constexpr int kSpacing[] = {-5, -6, 6};
      return kSpacing[mode];
    }
    case TAB_AFTER_TITLE_PADDING:
      return touch_optimized_material ? 8 : 4;
    case TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH:
      return 16;
    case TAB_ALERT_INDICATOR_ICON_WIDTH:
      return touch_optimized_material ? 12 : 16;
    case TAB_HEIGHT: {
      constexpr int kTabHeight[] = {29, 33, 41};
      return kTabHeight[mode];
    }
    case TAB_PRE_TITLE_PADDING:
      return touch_optimized_material ? 8 : 6;
    case TAB_STACK_DISTANCE:
      return touch_optimized_material ? 4 : 6;
    case TAB_STACK_TAB_WIDTH:
      return touch_optimized_material ? 68 : 120;
    case TAB_STANDARD_WIDTH:
      return touch_optimized_material ? 245 : 193;
    case TOOLBAR_ELEMENT_PADDING:
      return hybrid ? 8 : 0;
    case TOOLBAR_STANDARD_SPACING: {
      constexpr int kSpacings[] = {4, 8, 12};
      return kSpacings[mode];
    }
  }
  NOTREACHED();
  return 0;
}

gfx::Insets GetLayoutInsets(LayoutInset inset) {
  const int mode = ui::MaterialDesignController::GetMode();
  switch (inset) {
    case TAB: {
      constexpr int kTabHorizontalInset[] = {16, 18, 24};
      return gfx::Insets(1, kTabHorizontalInset[mode]);
    }
    case TOOLBAR_BUTTON:
      return gfx::Insets(
          ui::MaterialDesignController::IsTouchOptimizedUiEnabled() ? 12 : 6);

    case TOOLBAR_ACTION_VIEW: {
      // TODO(afakhry): Unify all toolbar button sizes on all platforms.
      // https://crbug.com/822967.
      constexpr int kToolbarActionsInsets[] = {2, 4, 10};
      return gfx::Insets(kToolbarActionsInsets[mode]);
    }
  }
  NOTREACHED();
  return gfx::Insets();
}

gfx::Size GetLayoutSize(LayoutSize size, bool is_incognito) {
  const int mode = ui::MaterialDesignController::GetMode();
  switch (size) {
    case NEW_TAB_BUTTON: {
      const gfx::Size sizes[] = {
          {36, 18}, {39, 21}, {(is_incognito ? 42 : 24), 24}};
      return sizes[mode];
    }
  }
  NOTREACHED();
  return gfx::Size();
}
