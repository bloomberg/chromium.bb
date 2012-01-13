// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_types.h"

namespace ash {

LauncherItem::LauncherItem()
    : type(TYPE_TABBED),
      window(NULL),
      num_tabs(1) {
}

LauncherItem::LauncherItem(LauncherItemType type, aura::Window* window)
    : type(type),
      window(window),
      num_tabs(1) {
}

LauncherItem::~LauncherItem() {
}

}  // namespace ash
