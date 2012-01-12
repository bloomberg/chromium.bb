// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_types.h"

namespace ash {

LauncherItem::LauncherItem()
    : type(TYPE_TABBED),
      window(NULL),
      user_data(NULL) {
}

LauncherItem::LauncherItem(LauncherItemType type,
                           aura::Window* window,
                           void* user_data)
    : type(type),
      window(window),
      user_data(user_data) {
}

LauncherItem::~LauncherItem() {
}

}  // namespace ash
