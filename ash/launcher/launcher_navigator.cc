// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_navigator.h"

#include "ash/launcher/launcher_model.h"

namespace ash {

namespace {

// Returns true if accelerator processing should skip the launcher item with
// the specified type.
bool ShouldSkip(ash::LauncherItemType type) {
  return type == ash::TYPE_APP_LIST ||
         type == ash::TYPE_BROWSER_SHORTCUT ||
         type == ash::TYPE_APP_SHORTCUT ||
         type == ash::TYPE_WINDOWED_APP;
}

}  // namespace

int GetNextActivatedItemIndex(const LauncherModel& model,
                              CycleDirection direction) {
  const ash::LauncherItems& items = model.items();
  int item_count = model.item_count();
  int current_index = -1;
  int first_running = -1;

  for (int i = 0; i < item_count; ++i) {
    const ash::LauncherItem& item = items[i];
    if (ShouldSkip(item.type))
      continue;

    if (item.status == ash::STATUS_RUNNING && first_running < 0)
      first_running = i;

    if (item.status == ash::STATUS_ACTIVE) {
      current_index = i;
      break;
    }
  }

  // If nothing is active, try to active the first running item.
  if (current_index < 0) {
    if (first_running >= 0)
      return first_running;
    else
      return -1;
  }

  int step = (direction == CYCLE_FORWARD) ? 1 : -1;

  // Find the next item and activate it.
  for (int i = (current_index + step + item_count) % item_count;
       i != current_index; i = (i + step + item_count) % item_count) {
    const ash::LauncherItem& item = items[i];
    if (ShouldSkip(item.type))
      continue;

    // Skip already active item.
    if (item.status == ash::STATUS_ACTIVE)
      continue;

    return i;
  }

  return -1;
}

}  // namespace ash
