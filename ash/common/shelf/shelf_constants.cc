// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_constants.h"

#include "ash/common/material_design/material_design_controller.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

const int kInvalidImageResourceID = -1;
const int kTimeToSwitchBackgroundMs = 1000;
const int kWorkspaceAreaVisibleInset = 2;
const int kWorkspaceAreaAutoHideInset = 5;
const int kShelfAutoHideSize = 3;
const int kShelfItemInset = 3;
const SkColor kShelfBaseColor = SK_ColorBLACK;
const SkColor kShelfButtonActivatedHighlightColor =
    SkColorSetA(SK_ColorWHITE, 100);
const float kShelfInkDropVisibleOpacity = 0.2f;
const SkColor kShelfIconColor = SK_ColorWHITE;
const int kOverflowButtonSize = 32;
const int kOverflowButtonCornerRadius = 2;
const int kAppListButtonRadius = kOverflowButtonSize / 2;

int GetShelfConstant(ShelfConstant shelf_constant) {
  const int kShelfBackgroundAlpha[] = {204, 204, 153};
  const int kShelfSize[] = {47, 47, 48};
  const int kShelfButtonSpacing[] = {10, 10, 16};
  const int kShelfButtonSize[] = {44, 44, 48};
  const int kShelfInsetsForAutoHide[] = {3, 3, 0};

  const int mode = MaterialDesignController::GetMode();
  DCHECK(mode >= MaterialDesignController::NON_MATERIAL &&
         mode <= MaterialDesignController::MATERIAL_EXPERIMENTAL);

  switch (shelf_constant) {
    case SHELF_BACKGROUND_ALPHA:
      return kShelfBackgroundAlpha[mode];
    case SHELF_SIZE:
      return kShelfSize[mode];
    case SHELF_BUTTON_SPACING:
      return kShelfButtonSpacing[mode];
    case SHELF_BUTTON_SIZE:
      return kShelfButtonSize[mode];
    case SHELF_INSETS_FOR_AUTO_HIDE:
      return kShelfInsetsForAutoHide[mode];
  }
  NOTREACHED();
  return 0;
}

}  // namespace ash
