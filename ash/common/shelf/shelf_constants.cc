// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_constants.h"

#include "ash/common/material_design/material_design_controller.h"
#include "base/logging.h"

namespace ash {

const int kInvalidImageResourceID = -1;
const int kShelfSize = 47;
const int kShelfButtonSpacing = 10;
const int kShelfButtonSize = 44;
const int kTimeToSwitchBackgroundMs = 1000;
const SkColor kShelfBaseColor = SK_ColorBLACK;

int GetShelfConstant(ShelfConstant shelf_constant) {
  const int kShelfBackgroundAlpha[] = {204, 153, 153};

  const int mode = MaterialDesignController::GetMode();
  DCHECK(mode >= MaterialDesignController::NON_MATERIAL &&
         mode <= MaterialDesignController::MATERIAL_EXPERIMENTAL);

  switch (shelf_constant) {
    case SHELF_BACKGROUND_ALPHA:
      return kShelfBackgroundAlpha[mode];
  }
  NOTREACHED();
  return 0;
}

}  // namespace ash
