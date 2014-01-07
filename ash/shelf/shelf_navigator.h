// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_NAVIGATOR_H_
#define ASH_SHELF_SHELF_NAVIGATOR_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_constants.h"

namespace ash {

class ShelfModel;

// Scans the current shelf item and returns the index of the shelf item which
// should be activated next for the specified |direction|. Returns -1 if fails
// to find such item.
ASH_EXPORT int GetNextActivatedItemIndex(const ShelfModel& model,
                                         CycleDirection direction);

}  // namespace ash

#endif  // ASH_SHELF_SHELF_NAVIGATOR_H_
