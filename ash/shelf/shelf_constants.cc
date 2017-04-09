// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_constants.h"

#include "base/logging.h"

namespace ash {

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
