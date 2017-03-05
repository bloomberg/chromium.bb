// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_constants.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

const int kTimeToSwitchBackgroundMs = 1000;
const int kWorkspaceAreaVisibleInset = 2;
const int kWorkspaceAreaAutoHideInset = 5;
const int kShelfAutoHideSize = 3;
const SkColor kShelfDefaultBaseColor = SK_ColorBLACK;
const int kShelfButtonSize = 48;
const int kShelfButtonSpacing = 16;
const SkColor kShelfButtonActivatedHighlightColor =
    SkColorSetA(SK_ColorWHITE, 100);
const SkColor kShelfInkDropBaseColor = SK_ColorWHITE;
const float kShelfInkDropVisibleOpacity = 0.2f;
const SkColor kShelfIconColor = SK_ColorWHITE;
const int kShelfTranslucentAlpha = 153;
const int kOverflowButtonSize = 32;
const int kOverflowButtonCornerRadius = 2;
const int kAppListButtonRadius = kOverflowButtonSize / 2;

int GetShelfConstant(ShelfConstant shelf_constant) {
  const int kShelfSize[] = {47, 48, 48};
  const int kShelfInsetsForAutoHide[] = {3, 0, 0};

  // TODO(estade): clean this up --- remove unneeded constants and reduce
  // remaining arrays to a single constant.
  const int mode = 1;
  switch (shelf_constant) {
    case SHELF_SIZE:
      return kShelfSize[mode];
    case SHELF_INSETS_FOR_AUTO_HIDE:
      return kShelfInsetsForAutoHide[mode];
  }
  NOTREACHED();
  return 0;
}

}  // namespace ash
