// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_MODEL_UTIL_H_
#define ASH_SHELF_SHELF_MODEL_UTIL_H_

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"

namespace ash {

class LauncherModel;

// Return the index of the |type| from a given |launcher_model|.
// Note:
//  * If there are many items for |type| in the |launcher_model|, an index of
//    first item will be returned.
//  * If there is no item for |type|, -1 will be returned.
ASH_EXPORT int GetShelfItemIndexForType(LauncherItemType type,
                                        const LauncherModel& launcher_model);

}  // namespace ash

#endif  // ASH_SHELF_SHELF_MODEL_UTIL_H_
