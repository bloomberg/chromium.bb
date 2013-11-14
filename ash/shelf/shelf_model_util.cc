// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_model_util.h"

#include "ash/shelf/shelf_model.h"

namespace ash {

int GetShelfItemIndexForType(LauncherItemType type,
                             const ShelfModel& shelf_model) {
  for (size_t i = 0; i < shelf_model.items().size(); i++) {
    if (shelf_model.items()[i].type == type)
      return i;
  }
  return -1;
}

}  // namespace ash
