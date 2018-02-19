// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/layout_constants.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "ui/base/material_design/material_design_controller.h"

int GetLayoutConstant(LayoutConstant constant) {
  const bool hybrid = ui::MaterialDesignController::GetMode() ==
                      ui::MaterialDesignController::MATERIAL_HYBRID;
  const bool touch_optimized_material =
      ui::MaterialDesignController::IsTouchOptimizedUiEnabled();

  switch (constant) {
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
    case LOCATION_BAR_HEIGHT:
      return hybrid ? 32 : 28;
    case LOCATION_BAR_ICON_SIZE:
      return 16;
    case LOCATION_BAR_ICON_INTERIOR_PADDING:
      return 4;
    case TABSTRIP_NEW_TAB_BUTTON_OVERLAP:
      return hybrid ? 6 : 5;
    case TAB_HEIGHT: {
      constexpr int kTabHeight[] = {29, 33, 41};
      return kTabHeight[ui::MaterialDesignController::GetMode()];
    }
    case TOOLBAR_ELEMENT_PADDING:
      return hybrid ? 8 : 0;
    case TOOLBAR_STANDARD_SPACING:
      return hybrid ? 8 : 4;
    case TAB_STACK_TAB_WIDTH:
      return touch_optimized_material ? 68 : 120;
    case TAB_STACK_DISTANCE:
      return touch_optimized_material ? 4 : 6;
    case TAB_STANDARD_WIDTH:
      return touch_optimized_material ? 245 : 193;
    case TAB_PRE_TITLE_PADDING:
      return touch_optimized_material ? 8 : 6;
    case TAB_AFTER_TITLE_PADDING:
      return touch_optimized_material ? 8 : 4;
    case TAB_ALERT_INDICATOR_ICON_WIDTH:
      return touch_optimized_material ? 12 : 16;
    case TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH:
      return 16;
  }
  NOTREACHED();
  return 0;
}

gfx::Insets GetLayoutInsets(LayoutInset inset) {
  switch (inset) {
    case TAB:
      constexpr int kTabHorizontalInset[] = {16, 18, 24};
      return gfx::Insets(
          1, kTabHorizontalInset[ui::MaterialDesignController::GetMode()]);
  }
  NOTREACHED();
  return gfx::Insets();
}

gfx::Size GetLayoutSize(LayoutSize size) {
  const bool hybrid = ui::MaterialDesignController::GetMode() ==
                      ui::MaterialDesignController::MATERIAL_HYBRID;
  switch (size) {
    case NEW_TAB_BUTTON:
      return hybrid ? gfx::Size(39, 21) : gfx::Size(36, 18);
  }
  NOTREACHED();
  return gfx::Size();
}
