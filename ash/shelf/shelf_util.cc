// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_util.h"

#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_types.h"

namespace ash {

int GetBrowserItemIndex(const LauncherModel& launcher_model) {
  for (size_t i = 0; i < launcher_model.items().size(); i++) {
    if (launcher_model.items()[i].type == ash::TYPE_BROWSER_SHORTCUT)
      return i;
  }
  return -1;
}

}  // namespace ash
