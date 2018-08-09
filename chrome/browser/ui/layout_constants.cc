// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/layout_constants.h"

#include "base/logging.h"
#include "ui/base/material_design/material_design_controller.h"

#if defined(OS_MACOSX)
int GetCocoaLayoutConstant(LayoutConstant constant) {
  switch (constant) {
    case BOOKMARK_BAR_HEIGHT:
      return 28;
    case BOOKMARK_BAR_NTP_HEIGHT:
      return 39;
    case BOOKMARK_BAR_HEIGHT_NO_OVERLAP:
      return GetCocoaLayoutConstant(BOOKMARK_BAR_HEIGHT) - 2;
    case BOOKMARK_BAR_NTP_PADDING:
      return (GetCocoaLayoutConstant(BOOKMARK_BAR_NTP_HEIGHT) -
              GetCocoaLayoutConstant(BOOKMARK_BAR_HEIGHT)) /
             2;
    default:
      return GetLayoutConstant(constant);
  }
}
#endif

int GetLayoutConstant(LayoutConstant constant) {
  const int mode = ui::MaterialDesignController::GetMode();
  const bool hybrid = mode == ui::MaterialDesignController::MATERIAL_HYBRID;
  const bool touch_optimized_material =
      ui::MaterialDesignController::IsTouchOptimizedUiEnabled();
  const bool newer_material = ui::MaterialDesignController::IsNewerMaterialUi();
  switch (constant) {
    case BOOKMARK_BAR_HEIGHT:
      return touch_optimized_material ? 40 : 32;
    case BOOKMARK_BAR_NTP_HEIGHT:
      return touch_optimized_material ? GetLayoutConstant(BOOKMARK_BAR_HEIGHT)
                                      : 39;
    case HOSTED_APP_MENU_BUTTON_SIZE:
      return 24;
    case HOSTED_APP_PAGE_ACTION_ICON_SIZE:
      // We must limit the size of icons in the title bar to avoid vertically
      // stretching the container view.
      return 16;
    case LOCATION_BAR_BUBBLE_VERTICAL_PADDING:
      return hybrid ? 1 : 3;
    case LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING:
      return hybrid ? 3 : 2;
    case LOCATION_BAR_BUBBLE_CORNER_RADIUS:
      // TODO(tapted): This should match BubbleBorder::GetBorderRadius() once
      // MD is default for secondary UI everywhere. That is, the constant should
      // move to views/layout_provider.h so that all bubbles are consistent.
      return newer_material ? 8 : 2;
    case LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET:
      return 1;
    case LOCATION_BAR_ELEMENT_PADDING: {
      const int kPadding[] = {1, 3, 3, 2, 3};
      return kPadding[mode];
    }
    case LOCATION_BAR_HEIGHT: {
      constexpr int kHeights[] = {28, 32, 36, 28, 36};
      return kHeights[mode];
    }
    case LOCATION_BAR_ICON_SIZE:
      return touch_optimized_material ? 20 : 16;
    case TAB_AFTER_TITLE_PADDING:
      return touch_optimized_material ? 8 : 4;
    case TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH:
      return 16;
    case TAB_ALERT_INDICATOR_ICON_WIDTH:
      return touch_optimized_material ? 12 : 16;
    case TAB_HEIGHT: {
      constexpr int kTabHeight[] = {29, 33, 41, 34, 41};
      return kTabHeight[mode] + GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
    }
    case TAB_PRE_TITLE_PADDING:
      return newer_material ? 8 : 6;
    case TAB_STACK_DISTANCE:
      return touch_optimized_material ? 4 : 6;
    case TABSTRIP_TOOLBAR_OVERLAP:
      return ui::MaterialDesignController::IsRefreshUi() ? 1 : 0;
    case TOOLBAR_ELEMENT_PADDING: {
      constexpr int kPadding[] = {0, 8, 0, 4, 0};
      return kPadding[mode];
    }
    case TOOLBAR_STANDARD_SPACING: {
      constexpr int kSpacings[] = {4, 8, 12, 8, 12};
      return kSpacings[mode];
    }
    default:
      break;
  }
  NOTREACHED();
  return 0;
}

gfx::Insets GetLayoutInsets(LayoutInset inset) {
  const int mode = ui::MaterialDesignController::GetMode();
  const bool touch_optimized_material =
      ui::MaterialDesignController::IsTouchOptimizedUiEnabled();
  switch (inset) {
    case LOCATION_BAR_ICON_INTERIOR_PADDING: {
      if (ui::MaterialDesignController::IsRefreshUi()) {
        return touch_optimized_material ? gfx::Insets(5, 10)
                                        : gfx::Insets(4, 8);
      }

      return touch_optimized_material ? gfx::Insets(5) : gfx::Insets(4);
    }

    case TOOLBAR_BUTTON:
      return gfx::Insets(touch_optimized_material ? 12 : 6);

    case TOOLBAR_ACTION_VIEW: {
      // TODO(afakhry): Unify all toolbar button sizes on all platforms.
      // https://crbug.com/822967.
      constexpr int kToolbarActionsInsets[] = {2, 4, 10, 2, 10};
      return gfx::Insets(kToolbarActionsInsets[mode]);
    }
  }
  NOTREACHED();
  return gfx::Insets();
}
